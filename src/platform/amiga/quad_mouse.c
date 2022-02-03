/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga quadrature mouse interface.
 */

#include "quad_mouse.h"
#include "util/output.h"

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

// default pins for amiga mouse communication
#ifndef PIN_AMIGA_MOUSE_H
#  define PIN_AMIGA_MOUSE_H 9   // pin 12, GP9
#endif
#ifndef PIN_AMIGA_MOUSE_V
#  define PIN_AMIGA_MOUSE_V 8   // pin 11, GP8
#endif
#ifndef PIN_AMIGA_MOUSE_HQ
#  define PIN_AMIGA_MOUSE_HQ 7   // pin 10, GP7
#endif
#ifndef PIN_AMIGA_MOUSE_VQ
#  define PIN_AMIGA_MOUSE_VQ 6   // pin 9, GP6
#endif
#ifndef PIN_AMIGA_MOUSE_B1
#  define PIN_AMIGA_MOUSE_B1 22   // pin 29, GP22
#endif
#ifndef PIN_AMIGA_MOUSE_B2
#  define PIN_AMIGA_MOUSE_B2 26   // pin 31, GP26
#endif
#ifndef PIN_AMIGA_MOUSE_B3
#  define PIN_AMIGA_MOUSE_B3 27   // pin 32, GP27
#endif
