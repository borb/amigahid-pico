/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga quadrature mouse interface.
 */

#ifndef _PLATFORM_AMIGA_QUAD_MOUSE_H
#define _PLATFORM_AMIGA_QUAD_MOUSE_H

#include <stdint.h>
#include <stdbool.h>

enum amiga_quad_mouse_buttons { AQM_LEFT, AQM_MIDDLE, AQM_RIGHT };

void amiga_quad_mouse_init();
void amiga_quad_mouse_motion();
void amiga_quad_mouse_button(enum amiga_quad_mouse_buttons button, bool pressed);
void amiga_quad_mouse_set_motion(int8_t in_x, int8_t in_y);

#endif
