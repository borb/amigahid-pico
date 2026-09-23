/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * bluetooth-safe declarations for shared hid input bridging.
 */

#ifndef _INPUT_BRIDGE_BT_H
#define _INPUT_BRIDGE_BT_H

#include <stdint.h>

#include "tusb_config.h"

#define INPUT_BRIDGE_BT_CLASSIC_SLOTS 2
#define INPUT_BRIDGE_BT_LE_SLOTS      1
#define INPUT_BRIDGE_MAX_SLOTS        (CFG_TUH_HID + INPUT_BRIDGE_BT_CLASSIC_SLOTS + INPUT_BRIDGE_BT_LE_SLOTS)

#define INPUT_BRIDGE_BT_CLASSIC_SLOT_BASE CFG_TUH_HID
#define INPUT_BRIDGE_BT_LE_SLOT_BASE      (INPUT_BRIDGE_BT_CLASSIC_SLOT_BASE + INPUT_BRIDGE_BT_CLASSIC_SLOTS)

void input_bridge_reset(uint8_t slot);
void input_bridge_disconnect(uint8_t slot);
void input_bridge_handle_keyboard_boot(uint8_t slot, uint8_t modifier, uint8_t const keycode[6]);
void input_bridge_handle_mouse_boot(uint8_t slot, uint8_t buttons, int8_t x, int8_t y, int8_t wheel);

#endif // _INPUT_BRIDGE_BT_H
