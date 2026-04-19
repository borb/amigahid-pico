/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga quadrature mouse interface.
 */

#include "config.h"
#include "quad_mouse.h"
#include "util/output.h"

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"

#define AQM_MOTION_QUEUE_DEPTH 32

typedef struct
{
    int16_t x;
    int16_t y;
} aqm_motion_t;

static queue_t motion_queue;
volatile uint8_t motion_divider = 2;

enum _mouse_pin_state { LOW, HIGH };

static inline int16_t _aqm_add_clamped(int16_t value, int16_t delta)
{
    int32_t sum = value + delta;

    if (sum > INT16_MAX)
        sum = INT16_MAX;
    else if (sum < INT16_MIN)
        sum = INT16_MIN;

    return (int16_t)sum;
}

static inline void _aqm_gpio_set(uint gpio, enum _mouse_pin_state state)
{
    if (state == LOW) {
        gpio_put(gpio, 0);
        gpio_set_dir(gpio, GPIO_OUT);
        return;
    }

    // assume it's high otherwise
    gpio_set_dir(gpio, GPIO_IN);
}

void amiga_quad_mouse_init()
{
    // obtain the pins we want to use
    gpio_init(QM1_AMIGA_H);
    gpio_init(QM1_AMIGA_V);
    gpio_init(QM1_AMIGA_HQ);
    gpio_init(QM1_AMIGA_VQ);
    gpio_init(QM1_AMIGA_B1);
    gpio_init(QM1_AMIGA_B2);
    gpio_init(QM1_AMIGA_B3);

    gpio_set_function(QM1_AMIGA_H, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_V, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_HQ, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_VQ, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B1, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B2, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B3, GPIO_FUNC_SIO);

    // pins are active low, so when they are at 0 they're triggering; set all high (off)
    _aqm_gpio_set(QM1_AMIGA_H, HIGH);
    _aqm_gpio_set(QM1_AMIGA_V, HIGH);
    _aqm_gpio_set(QM1_AMIGA_HQ, HIGH);
    _aqm_gpio_set(QM1_AMIGA_VQ, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B1, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B2, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B3, HIGH);

    queue_init(&motion_queue, sizeof(aqm_motion_t), AQM_MOTION_QUEUE_DEPTH);

    // start the mouse motion loop on core1
    multicore_launch_core1(amiga_quad_mouse_motion);
}

void amiga_quad_mouse_button(enum amiga_quad_mouse_buttons button, bool pressed)
{
    // ahprintf("[aqm] button %s state %s\n",
    //     (button == AQM_LEFT) ? "left" :
    //         (button == AQM_MIDDLE) ? "middle" :
    //         (button == AQM_RIGHT) ? "right" : "<unknown?!>",
    //     pressed ? "down" : "up"
    // );

    switch (button) {
        case AQM_LEFT:      _aqm_gpio_set(QM1_AMIGA_B1, pressed ? LOW : HIGH); break;
        case AQM_MIDDLE:    _aqm_gpio_set(QM1_AMIGA_B3, pressed ? LOW : HIGH); break;
        case AQM_RIGHT:     _aqm_gpio_set(QM1_AMIGA_B2, pressed ? LOW : HIGH); break;
        // default:            ahprintf("[aqm] unhandled button press!\n");
    }
}

void amiga_quad_mouse_set_motion(int16_t in_x, int16_t in_y)
{
    aqm_motion_t motion = { in_x, in_y };

    if (!queue_try_add(&motion_queue, &motion)) {
        aqm_motion_t merged;

        // When the queue is full, coalesce with a queued sample rather than
        // dropping motion outright.
        if (queue_try_remove(&motion_queue, &merged)) {
            merged.x = _aqm_add_clamped(merged.x, in_x);
            merged.y = _aqm_add_clamped(merged.y, in_y);

            if (queue_try_add(&motion_queue, &merged))
                return;
        }

        queue_try_add(&motion_queue, &motion);
    }
}

void amiga_quad_mouse_motion()
{
    // ahprintf("[aqm] hello from core1, mouse motion output loop starting\n");
    aqm_motion_t motion;
    int16_t out_x = 0, out_y = 0;
    int16_t x_residue = 0, y_residue = 0;
    uint8_t quad_mx_state = 1, quad_my_state = 1;
    uint8_t divider;

    /**
     * a little note about quadrature motion state.
     *
     * quadrature motion works by having a hardware-side counter for each axis and two signal
     * lines per axis. motion is signalled in an offset time division; the main axis pulse
     * changes state on time 0 and time 1, and the second signal line at time interval 0.5 and
     * 1.5, giving four possible states for each t/2. this occurs on both x and y axis.
     *
     * adcd has a crude ascii timing diagram but it explains it better:
     * https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017F.html
     */

    while (1) {
        // Merge newly queued USB deltas between quadrature steps so we do not
        // lose motion across cores or wait for an entire stale batch to drain.
        divider = motion_divider ? motion_divider : 1;

        while (queue_try_remove(&motion_queue, &motion)) {
            x_residue = _aqm_add_clamped(x_residue, motion.x);
            y_residue = _aqm_add_clamped(y_residue, motion.y);
        }

        out_x = _aqm_add_clamped(out_x, x_residue / divider);
        out_y = _aqm_add_clamped(out_y, y_residue / divider);
        x_residue %= divider;
        y_residue %= divider;

        if ((out_x == 0) && (out_y == 0)) {
            tight_loop_contents();
            continue;
        }

        if (out_x != 0) {
            // handle x-axis motion
            if (out_x < 0)
                quad_mx_state--;
            else if (out_x > 0)
                quad_mx_state++;
            // fix wraparound
            if (quad_mx_state == 255)
                quad_mx_state = 3;
            else if (quad_mx_state == 4)
                quad_mx_state = 0;

            switch (quad_mx_state) {
                case 0: _aqm_gpio_set(QM1_AMIGA_H, HIGH); break;
                case 1: _aqm_gpio_set(QM1_AMIGA_HQ, HIGH); break;
                case 2: _aqm_gpio_set(QM1_AMIGA_H, LOW); break;
                case 3: _aqm_gpio_set(QM1_AMIGA_HQ, LOW); break;
            }
        }

        if (out_x < 0) out_x++;
        if (out_x > 0) out_x--;

        if (out_y != 0) {
            // handle y-axis motion
            if (out_y < 0)
                quad_my_state--;
            else if (out_y > 0)
                quad_my_state++;
            // fix wraparound
            if (quad_my_state == 255)
                quad_my_state = 3;
            else if (quad_my_state == 4)
                quad_my_state = 0;

            switch (quad_my_state) {
                case 0: _aqm_gpio_set(QM1_AMIGA_V, HIGH); break;
                case 1: _aqm_gpio_set(QM1_AMIGA_VQ, HIGH); break;
                case 2: _aqm_gpio_set(QM1_AMIGA_V, LOW); break;
                case 3: _aqm_gpio_set(QM1_AMIGA_VQ, LOW); break;
            }
        }

        if (out_y < 0) out_y++;
        if (out_y > 0) out_y--;

        sleep_us(300); // delay before next iteration to prevent missing state change
    }
}
