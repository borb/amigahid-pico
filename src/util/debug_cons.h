/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * debug console routines.
 */

#ifndef _PLATFORM_COMMON_DEBUG_CONS_H
#define _PLATFORM_COMMON_DEBUG_CONS_H

#include <stdint.h>

enum debug_plug_types { AP_H_UNKNOWN, AP_H_KEYBOARD, AP_H_MOUSE, AP_H_CONTROLLER };

#define ESC         "\033"

#define VT_ED_EOS   ESC "[0J"
#define VT_ED_BOS   ESC "[1J"
#define VT_ED_CLS   ESC "[2J"
#define VT_EL_EOL   ESC "[0K"
#define VT_EL_BOL   ESC "[1K"
#define VT_EL_LIN   ESC "[2K"

#define VT_CUP_HOM  ESC "[H"
#define VT_CUP_POS  ESC "[%d;%dH"

void dbgcons_init();

void dbgcons_print_counters();

void dbgcons_plug(enum debug_plug_types devtype);

void dbgcons_unplug(enum debug_plug_types devtype);

void dbgcons_amiga_key(uint8_t incode, uint8_t outcode, char *updown);

#endif // _PLATFORM_COMMON_DEBUG_CONS_H
