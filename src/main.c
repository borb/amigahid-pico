/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * main entry point for amigahid-pico.
 */

// these reside within the tinyusb sdk and are not part of this project source
#include "bsp/board.h"
#include "tusb.h"

#include "platform/amiga/keyboard_serial_io.h"
#include "platform/amiga/quad_mouse.h"
#include "util/debug_cons.h"
#include "util/output.h"

#include "config.h"
#include "tusb_config.h"

// defined within usb_hid.c
extern void hid_app_task(void);

// main entry point
int main(void)
{
    // @todo put this somewhere else
    gpio_set_function(KBD_AMIGA_CLK, GPIO_FUNC_SIO);
    gpio_set_function(KBD_AMIGA_DAT, GPIO_FUNC_SIO);

    // i2c things here pls

    // tinyusb board init; led, uart, button, usb
    board_init();

    // say hello, trevor ("hello, trevor")
    dbgcons_init();

    // initialise the usb stack
    // defining CFG_TUSB_RHPORT0_MODE as OPT_MODE_HOST will put the controller into host mode
    tusb_init();

    // we're single arch right now, but in future this should hand off to whatever the
    // configured arch is
    amiga_init();

    // start amiga mouse emulation
    amiga_quad_mouse_init();

    while (1) {
        // run host mode jobs (hotplug events, packet io callbacks)
        tuh_task();

        // amiga keyboard service routine
        amiga_service();
    }

    return 0;
}
