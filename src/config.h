/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * hid-pico configuration
 *
 * PLEASE NOTE ALL PIN DESIGNATIONS ARE GPIO PIN NUMBERS - NOT PHYSICAL PIN NUMBERS
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#ifndef HIDPICO_REVISION
#  define HIDPICO_REVISION 4
#endif

#if HIDPICO_REVISION == 2
#  define I2C_PORT      i2c0
#  define I2C_PIN_SDA   4
#  define I2C_PIN_SCL   5
#  define I2C_IRQN      23

#  define KBD_AMIGA_RST 10
#  define KBD_AMIGA_DAT 11
#  define KBD_AMIGA_CLK 12

#  define QM1_AMIGA_HQ  7
#  define QM1_AMIGA_VQ  6
#  define QM1_AMIGA_H   9
#  define QM1_AMIGA_V   8
#  define QM1_AMIGA_B1  22
#  define QM1_AMIGA_B2  26
#  define QM1_AMIGA_B3  27
#elif HIDPICO_REVISION == 4
#  define I2C_PORT      i2c1
#  define I2C_PIN_SDA   2
#  define I2C_PIN_SCL   3
#  define I2C_IRQN      24 // this is tied to the i2c port being used so get the right one!

#  define KBD_AMIGA_RST 4
#  define KBD_AMIGA_DAT 5
#  define KBD_AMIGA_CLK 6

#  define QM1_AMIGA_HQ  7
#  define QM1_AMIGA_VQ  8
#  define QM1_AMIGA_H   9
#  define QM1_AMIGA_V   10
#  define QM1_AMIGA_B1  11
#  define QM1_AMIGA_B2  12
#  define QM1_AMIGA_B3  13
#endif

#endif // _CONFIG_H
