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

#include <stdio.h>
#include <stdarg.h>

#include "output.h"

void ahprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    ahvfprintf(stdout, fmt, args);

    va_end(args);
}

void ahfprintf(FILE *stream, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    ahvfprintf(stream, fmt, args);

    va_end(args);
}

void ahvfprintf(FILE *stream, const char *fmt, va_list args)
{
#ifdef DEBUG_MESSAGES
    vfprintf(stream, fmt, args);
#endif
}
