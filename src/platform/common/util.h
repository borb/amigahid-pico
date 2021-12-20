/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * common utility routines.
 */

#ifndef _PLATFORM_COMMON_UTIL_H
#define _PLATFORM_COMMON_UTIL_H

#include <stdbool.h>
#include <stdint.h>

bool key_in_buffer(uint8_t keycode, uint8_t len, uint8_t *buf);

#endif // _PLATFORM_COMMON_UTIL_H
