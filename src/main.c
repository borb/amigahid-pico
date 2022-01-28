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
#include "util/output.h"

#include "tusb_config.h"

// defined within usb_hid.c
extern void hid_app_task(void);

// welcome message
// output in this firmware will only occur when the following is present in
// src/CMakeLists.txt:
//  pico_enable_stdio_uart(amigahid-pico 1)
static const char welcomeText[] =
    "amigahid-pico by nine <nine@aphlor.org>, https://github.com/borb/amigahid-pico\n" \
    "this is free software; the software source and any schematic should be\n" \
    "made available to you free of charge. if you have not been provided with the\n" \
    "source code or schematic please report where you obtained the software and/or\n" \
    "hardware to the above address.\n\n";

// main entry point
int main(void)
{
    // tinyusb board init; led, uart, button, usb
    board_init();

    // say hello, trevor ("hello, trevor")
    ahprintf(welcomeText);

    // initialise the usb stack
    // definition CFG_TUSB_RHPORT0_MODE as OPT_MODE_HOST will put the board into host mode
    tusb_init();

    // we're single arch right now, but in future this should hand off to whatever the
    // configured arch is
    amiga_init();

    while (1) {
        // run host mode jobs (hotplug events, packet io callbacks)
        tuh_task();

        // amiga keyboard service routine
        amiga_service();
    }

    return 0;
}
