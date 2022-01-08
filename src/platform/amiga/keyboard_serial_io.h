/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga keyboard serial interface.
 */

#ifndef _PLATFORM_AMIGA_KEYBOARD_SERIAL_IO_H
#define _PLATFORM_AMIGA_KEYBOARD_SERIAL_IO_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Setup Amiga keyboard communication
 *
 * Starts clocks, sets up ISRs and sends init codes to the host
 */
void amiga_init();

/**
 * @brief get the caps lock status
 *
 * @return true     Caps lock is on
 * @return false    Caps lock is off
 */
bool amiga_caps_lock();

/**
 * @brief Send a keycode to the Amiga
 *
 * @param keycode   Keycode to send to the host
 * @param up        Boolean press status; if true, code is & 0x80 before rol
 */
void amiga_send(uint8_t keycode, bool up);

/**
 * @brief Assert the reset signal - Amiga will hold in reset until released
 */
void amiga_assert_reset();

/**
 * @brief Release reset signal - Amiga will initiate startup once released
 */
void amiga_release_reset();

/**
 * @brief Regular jobs to run whilst passing through the event check loop;
 *        keep host maintained so that it believes a keyboard is still
 *        attached
 */
void amiga_service();

#endif // _PLATFORM_AMIGA_KEYBOARD_SERIAL_IO_H
