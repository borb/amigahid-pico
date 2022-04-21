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
volatile int8_t x = 0, y = 0;
volatile bool motion_flag = false;
volatile uint8_t motion_divider = 2;

enum _mouse_pin_state { LOW, HIGH };

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

    // start the mouse motion loop on core1
    multicore_launch_core1(amiga_quad_mouse_motion);
}

void amiga_quad_mouse_button(enum amiga_quad_mouse_buttons button, bool pressed)
{
    ahprintf("[aqm] button %s state %s\n",
        (button == AMQ_LEFT) ? "left" :
            (button == AMQ_MIDDLE) ? "middle" :
            (button == AMQ_RIGHT) ? "right" : "<unknown?!>",
        pressed ? "down" : "up"
    );

    switch (button) {
        case AMQ_LEFT:      _aqm_gpio_set(QM1_AMIGA_B1, pressed ? LOW : HIGH); break;
        case AMQ_MIDDLE:    _aqm_gpio_set(QM1_AMIGA_B3, pressed ? LOW : HIGH); break;
        case AMQ_RIGHT:     _aqm_gpio_set(QM1_AMIGA_B2, pressed ? LOW : HIGH); break;
        default:            ahprintf("[aqm] unhandled button press!\n");
    }
}

void amiga_quad_mouse_set_motion(int8_t in_x, int8_t in_y)
{
    x = in_x;
    y = in_y;
    motion_flag = true;

    // @todo use fifo write here to unblock core1 thread?
}

void amiga_quad_mouse_motion()
{
    ahprintf("[aqm] hello from core1, mouse motion output loop starting\n");
    int8_t out_x, out_y;
    uint8_t quad_mx_state = 0, quad_my_state = 0;
    bool motion_x_skip = false, motion_y_skip = false;

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
        // @todo use blocking fifo read here to prevent wasting cycles?
        out_x = x;
        out_y = y;
        x = y = 0;
        motion_flag = false;

        while (((out_x != 0) || (out_y != 0)) && !motion_flag) {
            motion_x_skip = false;
            motion_y_skip = false;

            if ((out_x % motion_divider) != 0)
                motion_x_skip = true;
            if ((out_y % motion_divider) != 0)
                motion_y_skip = true;

            if ((out_x != 0) && !motion_x_skip) {
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

            if ((out_y != 0) && !motion_y_skip) {
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

            //if (motion_x_skip && motion_y_skip)
            //    continue;

            sleep_us(300); // delay before next iteration to prevent missing state change
        }
    }
}
