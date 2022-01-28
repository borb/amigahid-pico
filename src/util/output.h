/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * output handling. long-term, this should help us redirect output to i2c
 * displays but right now, it allows us to log to uart and turn that on and
 * off.
 */

#ifndef _UTIL_OUTPUT_H
#define _UTIL_OUTPUT_H

#include <stdio.h>
#include <stdarg.h>

void ahprintf(const char *fmt, ...);
void ahfprintf(FILE *stream, const char *fmt, ...);
void ahvfprintf(FILE *stream, const char *fmt, va_list args);

#endif
