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

#include "display/disp_ssd.h"
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
    // tinyusb board init; led, uart, button, usb
    board_init();

    // initialise the i2c controller and send the init sequence to the display
    disp_ssd_init();

    // say hello, trevor ("hello, trevor")
    dbgcons_init();

    // initialise the usb host stack on the rhport from tusb_config.h
    tuh_init(BOARD_TUH_RHPORT);

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
