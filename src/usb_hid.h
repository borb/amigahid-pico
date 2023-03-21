/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * usb_hid structures and definitions.
 *
 * part of the structures and code in this file have been repurposed from
 * https://github.com/fruit-bat/tinyusb in the hid_micro_parser branch, until
 * such a time that a) the PR is accepted upstream or b) tinyusb has its own
 * parser.
 */

#ifndef _USB_HID_H
#define _USB_HID_H

#include <stdint.h>

#define HID_RIP_EUSAGE(G, L) ((G << 16) | L)

typedef enum tuh_hid_rip_status {
    HID_RIP_INIT = 0,              // Initial state
    HID_RIP_EOF,                   // No more items
    HID_RIP_TTEM_OK,               // Last item parsed ok
    HID_RIP_ITEM_ERR,              // Issue decoding a single report item
    HID_RIP_STACK_OVERFLOW,        // Too many pushes
    HID_RIP_STACK_UNDERFLOW,       // Too many pops
    HID_RIP_USAGES_OVERFLOW,       // Too many usages
    HID_RIP_COLLECTIONS_OVERFLOW,  // Too many collections
    HID_RIP_COLLECTIONS_UNDERFLOW  // More collection ends than starts
} tuh_hid_rip_status_t;

typedef struct tuh_hid_rip_state {
    const uint8_t* cursor;
    uint16_t length;
    int16_t item_length;
    uint8_t stack_index;
    uint8_t usage_count;
    uint8_t collections_count;
    const uint8_t* global_items[HID_REPORT_STACK_SIZE][16];
    const uint8_t* local_items[16];
    const uint8_t* collections[HID_REPORT_MAX_COLLECTION_DEPTH];
    uint32_t usages[HID_REPORT_MAX_USAGES];
    tuh_hid_rip_status_t status;
} tuh_hid_rip_state_t;

#endif _USB_HID_H
