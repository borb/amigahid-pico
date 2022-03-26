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

// should be defined by the pico board.h
#ifndef CFG_TUSB_MCU
#   error CFG_TUSB_MCU is missing; please check your configuration (ported to another board?)
#endif

// stack in host mode (on the pico, can be host or device, not both [otg])
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_HOST

// BEGIN: borrowed from tusb config; may not have relevance on this platform
#ifndef CFG_TUSB_OS
#   define CFG_TUSB_OS OPT_OS_NONE
#endif

#ifndef CFG_TUSB_MEM_SECTION
#   define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#   define CFG_TUSB_MEM_ALIGN __attribute__ ((aligned(4)))
#endif
// END: borrowed from tusb config

// usb descriptor buffer size
#define CFG_TUH_ENUMERATION_BUFSIZE 512

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
