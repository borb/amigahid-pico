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
#include "hardware/gpio.h"

// mouse motion values, used between core0 and core1
volatile int16_t x = 0, y = 0;
volatile uint8_t motion_divider = 2;

enum _mouse_pin_state { LOW, HIGH };

static inline int16_t _aqm_accumulate_motion(volatile int16_t *axis, int16_t delta)
{
    int32_t sum = *axis + delta;

    if (sum > INT16_MAX)
        sum = INT16_MAX;
    else if (sum < INT16_MIN)
        sum = INT16_MIN;

    *axis = (int16_t)sum;
    return *axis;
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

static inline void _aqm_quad_step(uint gpio, uint gpio_q, uint8_t *state, int16_t motion)
{
    *state = (*state + (motion < 0 ? 3 : 1)) & 3;

    // Set both levels for the destination state. Updating only the pin that
    // changes during forward motion loses the first step after a reversal.
    // States (main, quadrature): 0 = 10, 1 = 11, 2 = 01, 3 = 00.
    _aqm_gpio_set(gpio, *state < 2 ? HIGH : LOW);
    _aqm_gpio_set(gpio_q, (*state == 1 || *state == 2) ? HIGH : LOW);
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
    _aqm_accumulate_motion(&x, in_x);
    _aqm_accumulate_motion(&y, in_y);
}

void amiga_quad_mouse_motion()
{
    // ahprintf("[aqm] hello from core1, mouse motion output loop starting\n");
    int16_t out_x, out_y;
    int16_t in_x, in_y;
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
        // Batch new deltas so slow axis-only motion is not lost when we divide
        // high-DPI mouse movement down to Amiga quadrature steps.
        in_x = x;
        in_y = y;
        x = y = 0;
        divider = motion_divider ? motion_divider : 1;

        x_residue += in_x;
        y_residue += in_y;
        out_x = x_residue / divider;
        out_y = y_residue / divider;
        // Retain sub-step motion for the next report, but do not add the
        // already-consumed whole steps again on the next loop iteration.
        x_residue %= divider;
        y_residue %= divider;

        while ((out_x != 0) || (out_y != 0)) {
            if (out_x != 0)
                _aqm_quad_step(QM1_AMIGA_H, QM1_AMIGA_HQ, &quad_mx_state, out_x);

            if (out_x < 0) out_x++;
            if (out_x > 0) out_x--;

            if (out_y != 0)
                _aqm_quad_step(QM1_AMIGA_V, QM1_AMIGA_VQ, &quad_my_state, out_y);

            if (out_y < 0) out_y++;
            if (out_y > 0) out_y--;

            sleep_us(300); // delay before next iteration to prevent missing state change
        }
    }
}
