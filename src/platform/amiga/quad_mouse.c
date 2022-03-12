/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga quadrature mouse interface.
 */

#include "quad_mouse.h"
#include "util/output.h"

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

// default pins for amiga mouse communication
#ifndef PIN_AMIGA_MOUSE_H
#  define PIN_AMIGA_MOUSE_H 9   // pin 12, GP9
#endif
#ifndef PIN_AMIGA_MOUSE_V
#  define PIN_AMIGA_MOUSE_V 8   // pin 11, GP8
#endif
#ifndef PIN_AMIGA_MOUSE_HQ
#  define PIN_AMIGA_MOUSE_HQ 7   // pin 10, GP7
#endif
#ifndef PIN_AMIGA_MOUSE_VQ
#  define PIN_AMIGA_MOUSE_VQ 6   // pin 9, GP6
#endif
#ifndef PIN_AMIGA_MOUSE_B1
#  define PIN_AMIGA_MOUSE_B1 22   // pin 29, GP22
#endif
#ifndef PIN_AMIGA_MOUSE_B2
#  define PIN_AMIGA_MOUSE_B2 26   // pin 31, GP26
#endif
#ifndef PIN_AMIGA_MOUSE_B3
#  define PIN_AMIGA_MOUSE_B3 27   // pin 32, GP27
#endif

// mouse motion values, used between core0 and core1
volatile int8_t x = 0, y = 0;
volatile bool motion_flag = false;
volatile uint8_t motion_divider = 2;

void amiga_quad_mouse_init()
{
    // obtain the pins we want to use
    gpio_init(PIN_AMIGA_MOUSE_H);
    gpio_init(PIN_AMIGA_MOUSE_V);
    gpio_init(PIN_AMIGA_MOUSE_HQ);
    gpio_init(PIN_AMIGA_MOUSE_VQ);
    gpio_init(PIN_AMIGA_MOUSE_B1);
    gpio_init(PIN_AMIGA_MOUSE_B2);
    gpio_init(PIN_AMIGA_MOUSE_B3);

    // set direction (all mouse functions are output only; anecdotally, all
    // buttons are on bidirectional contacts, and buttons 3 and 2 are pot
    // inputs)
    gpio_set_dir(PIN_AMIGA_MOUSE_H, GPIO_OUT);
    gpio_set_dir(PIN_AMIGA_MOUSE_V, GPIO_OUT);
    gpio_set_dir(PIN_AMIGA_MOUSE_HQ, GPIO_OUT);
    gpio_set_dir(PIN_AMIGA_MOUSE_VQ, GPIO_OUT);
    gpio_set_dir(PIN_AMIGA_MOUSE_B1, GPIO_OUT);
    gpio_set_dir(PIN_AMIGA_MOUSE_B2, GPIO_OUT);
    gpio_set_dir(PIN_AMIGA_MOUSE_B3, GPIO_OUT);

    // pins are active low, so when they are at 0 they're triggering; set all high (off)
    gpio_put(PIN_AMIGA_MOUSE_H, 1);
    gpio_put(PIN_AMIGA_MOUSE_V, 1);
    gpio_put(PIN_AMIGA_MOUSE_HQ, 1);
    gpio_put(PIN_AMIGA_MOUSE_VQ, 1);
    gpio_put(PIN_AMIGA_MOUSE_B1, 1);
    gpio_put(PIN_AMIGA_MOUSE_B2, 1);
    gpio_put(PIN_AMIGA_MOUSE_B3, 1);

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
        case AMQ_LEFT:      gpio_put(PIN_AMIGA_MOUSE_B1, pressed ? 0 : 1); break;
        case AMQ_MIDDLE:    gpio_put(PIN_AMIGA_MOUSE_B3, pressed ? 0 : 1); break;
        case AMQ_RIGHT:     gpio_put(PIN_AMIGA_MOUSE_B2, pressed ? 0 : 1); break;
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
                    case 0: gpio_put(PIN_AMIGA_MOUSE_H, 1); break;
                    case 1: gpio_put(PIN_AMIGA_MOUSE_HQ, 1); break;
                    case 2: gpio_put(PIN_AMIGA_MOUSE_H, 0); break;
                    case 3: gpio_put(PIN_AMIGA_MOUSE_HQ, 0); break;
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
                    case 0: gpio_put(PIN_AMIGA_MOUSE_V, 1); break;
                    case 1: gpio_put(PIN_AMIGA_MOUSE_VQ, 1); break;
                    case 2: gpio_put(PIN_AMIGA_MOUSE_V, 0); break;
                    case 3: gpio_put(PIN_AMIGA_MOUSE_VQ, 0); break;
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
