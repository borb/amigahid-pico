/**
 * this file is part of amigahid-pico, (c) 2023 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * usb stack configuration; this file is included from the project source
 * by the tinyusb sdk so must be available within the include path
 */

#ifndef _USB_HID_H
#define _USB_HID_H

// usb report identifier bytes
#define USAGE_MOUSE                 0x02
#define USAGE_PAGE_BUTTON           0x09
#define USAGE_PAGE_GENERIC_DCTRL    0x01
#define USAGE_X                     0x30
#define USAGE_Y                     0x31
#define USAGE_SCROLL_WHEEL          0x38

#endif // _USB_HID_H
