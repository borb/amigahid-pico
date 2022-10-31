/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * usb stack configuration; this file is included from the project source
 * by the tinyusb sdk so must be available within the include path
 */

#ifndef _TUSB_CONFIG_H
#define _TUSB_CONFIG_H

// rhport setting
#ifndef BOARD_TUH_RHPORT
#   define BOARD_TUH_RHPORT 0
#endif

// maximum speed permitted (which on a pico is usb 1.1)
#ifndef BOARD_TUH_MAX_SPEED
#   define BOARD_TUH_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

// should be defined by the pico board.h
#ifndef CFG_TUSB_MCU
#   error CFG_TUSB_MCU is missing; please check your configuration (ported to another board?)
#endif

#ifndef CFG_TUSB_OS
#   define CFG_TUSB_OS OPT_OS_NONE
#endif

// enable the host stack (if not explicitly enabled)
#ifndef CFG_TUH_ENABLED
#   define CFG_TUH_ENABLED 1
#endif

// max speed from board
#define CFG_TUH_MAX_SPEED BOARD_TUH_MAX_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#   define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#   define CFG_TUSB_MEM_ALIGN __attribute__ ((aligned(4)))
#endif

// usb descriptor buffer size
#define CFG_TUH_ENUMERATION_BUFSIZE 256

// supported devices & interfaces; tusb examples use a convention of multi-purpose
// constants where >0 may also mean multiple endpoints on a single device
#define CFG_TUH_HUB 4 // permit usb hubs (pico has a single port)
#define CFG_TUH_CDC 0 // no serial
#define CFG_TUH_HID 4 // keyboard/mouse/joystick; 4 endpoints maximum per device
#define CFG_TUH_MSC 0 // no mass storage
#define CFG_TUH_VENDOR 0 // not sure what this is but no anyway

// max device support (excluding hub)
#define CFG_TUH_DEVICE_MAX 16

// hid event buffer sizes
#define CFG_TUH_HID_EPIN_BUFSIZE 64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

#endif // _TUSB_CONFIG_H
