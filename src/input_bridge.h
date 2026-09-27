/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * shared hid input bridging for usb and bluetooth sources.
 */

#ifndef _INPUT_BRIDGE_H
#define _INPUT_BRIDGE_H

#include "input_bridge_bt.h"
#include "tusb.h"

typedef void (*input_bridge_set_leds_fn_t)(void *ctx, uint8_t led_report);

typedef struct
{
    input_bridge_set_leds_fn_t set_leds;
    void *ctx;
} input_bridge_keyboard_sink_t;

void input_bridge_reset(uint8_t slot);
void input_bridge_disconnect(uint8_t slot);
void input_bridge_handle_keyboard(uint8_t slot, hid_keyboard_report_t const *report,
    input_bridge_keyboard_sink_t const *sink);
void input_bridge_handle_mouse(uint8_t slot, hid_mouse_report_t const *report);

#endif // _INPUT_BRIDGE_H
