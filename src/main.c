/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * main entry point for amigahid-pico.
 */

#include <stdbool.h>

// these reside within the tinyusb sdk and are not part of this project source
#include "bsp/board_api.h"
#include "tusb.h"
#include "pico/platform.h"

#include "display/disp_ssd.h"
#include "platform/amiga/keyboard_serial_io.h"
#include "platform/amiga/quad_mouse.h"
#include "util/debug_cons.h"
#include "util/output.h"

#include "config.h"
#include "tusb_config.h"
#include "gen_version.h"

// defined within usb_hid.c
extern void hid_app_task(void);

// main entry point
int main(void)
{
    // tinyusb board init; led, uart, button, usb
    board_init();

    // greet the serial console
    ahprintf("[amiga?]hid-pico version %s starting up.\n"
             "This software and reference hardware design is free and licensed under EPL-2.0.\n"
             "Don't be scalped - this is not a commercial product.\n"
             "Bugs, suggestions, or just say hello: https://github.com/borb/amigahid-pico\n\n",
             _HP_VERSION);

    // initialise the i2c controller and send the init sequence to the display
    disp_ssd_init();

    // init i2c display and show initial information
    dbgcons_init();

    // initialise the usb host stack on the rhport from tusb_config.h
    ahprintf("About to init USB stack.\n");
    // @todo this is not the right place for this: needs moving... somewhere?
    tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);
    tuh_init(BOARD_TUH_RHPORT);
    ahprintf("USB stack init complete.\n\n");

    // we're single arch right now, but in future this should hand off to whatever the
    // configured arch is
    ahprintf("About to init Amiga keyboard sio communication.\n");
    amiga_init();
    ahprintf("Amiga keyboard sio init complete.\n\n");

    // start amiga mouse emulation
    ahprintf("About to init Amiga quadrature mouse emulation.\n");
    amiga_quad_mouse_init();
    ahprintf("Amiga quadrature mouse emulation init complete.\n\n");

    ahprintf("Going into main operation loop now.\n");
    while (true) {
        // run host mode jobs (hotplug events, packet io callbacks)
        tuh_task();

        // run hid tasks
        hid_app_task();

        // amiga keyboard service routine
        amiga_service();

        // this is a tight hardware polling loop, so run the tight_loop_contents no-op
        tight_loop_contents();
    }

    return 0;
}
