/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * human interface device handling.
 *
 * if this file looks kind of sketchy, i'm just at the beginning of my
 * understanding of the tinyusb stack. apologies whilst i get to grips with
 * the terminology, or mapping it to what i understand of it.
 *
 * part of the structures and code in this file have been repurposed from
 * https://github.com/fruit-bat/tinyusb in the hid_micro_parser branch, until
 * such a time that a) the PR is accepted upstream or b) tinyusb has its own
 * parser.
 */

// these reside within the tinyusb sdk and are not part of this project source
#include "bsp/board.h"
#include "tusb.h"

// other includes
#include <stdint.h>
#include <stdlib.h>

#include "tusb_config.h"
#include "platform/amiga/keyboard_serial_io.h"  // amiga only, for now, until i get hold of an ST :D
#include "platform/amiga/keyboard.h"
#include "platform/amiga/quad_mouse.h"
#include "util/debug_cons.h"

// maximum number of reports per hid device
#define MAX_REPORT 4

// repetitive modifier check macros (@todo probably better iterated in future?)
#define _SINGLE_MOD_CHECK(hid_mod) \
    if ((report->modifier & hid_mod) && !(last_report.modifier & hid_mod)) \
        amiga_hid_modifier(hid_mod, false); \
    if (!(report->modifier & hid_mod) && (last_report.modifier & hid_mod)) \
        amiga_hid_modifier(hid_mod, true);

#define _MULTI_MOD_CHECK(hid_mod_a, hid_mod_b) \
    if (((report->modifier & hid_mod_a) && !(last_report.modifier & hid_mod_a)) \
        || ((report->modifier & hid_mod_b) && !(last_report.modifier & hid_mod_b)) \
    ) \
        amiga_hid_modifier(hid_mod_a, false); \
    if ((!(report->modifier & hid_mod_a) && (last_report.modifier & hid_mod_a)) \
        || (!(report->modifier & hid_mod_b) && (last_report.modifier & hid_mod_b)) \
    ) \
        amiga_hid_modifier(hid_mod_a, true);

// textual representations of attached devices
const uint8_t hid_protocol_type[] = { AP_H_UNKNOWN, AP_H_KEYBOARD, AP_H_MOUSE };

// hid information structure
static struct _hid_info
{
    uint8_t report_count;
    tuh_hid_report_info_t report_info[MAX_REPORT];
    uint8_t *desc_report;
    uint16_t desc_len;
} hid_info[CFG_TUH_HID];

static void process_report(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len);
static void handle_event_keyboard(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report);
static void handle_event_mouse(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const *report);

void hid_app_task(void)
{
    // null function to satisfy stack
}

// callback functions; methods below suffixed with "_cb" are called by tinyusb
// when processing certain hid events.

/**
 * HID connection callback
 *
 * @param dev_addr    Address of connected device
 * @param instance    Instance of connected device
 * @param desc_report Report descriptor
 * @param desc_len    Length of descriptor
 */
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, const uint8_t *desc_report, uint16_t desc_len)
{
    uint8_t hid_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    char msgbuf[120] = "";

    dbgcons_plug(hid_protocol_type[hid_protocol]);

    if (desc_len > 0) {
        hid_info[instance].desc_report = malloc(desc_len);
        hid_info[instance].desc_len = desc_len;
        memcpy(hid_info[instance].desc_report, desc_report, (size_t) desc_len);

        sprintf(msgbuf, "Copied report descriptor of size 0x%x to address 0x%x\n", desc_len, (unsigned int) desc_report);
        dbgcons_message(msgbuf);
    }

    /**
     * trying to understand this. don't fully fathom this yet but working on it.
     *
     * if hid_protocol is keyboard or mouse then it's probably a simple hidbp device.
     *
     * if it has no protocol, the protocol is implemented at interface level, and each interface comes with a descriptor
     * (and can also be in hidbp mode until switched out of it, e.g. combined kb/mouse in boot mode).
     *
     * this code says "if you are hid but have no protocol, let's look at your descriptor block".
     * tuh_hid_parse_report_descriptor() extracts the device type (kb/mouse/controller/etc) from the usage page but is
     * simplistic and throws the rest of the data away.
     * we need to parse desc_report when a report arrives in order to extract the data we want. i don't think this
     * data is available after this moment in time, so copy it into hid_info[n]
     */
    if (hid_protocol == HID_ITF_PROTOCOL_NONE) {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
    }

    // switch mouse into report mode (out of hidbp)
    if (hid_protocol == HID_ITF_PROTOCOL_MOUSE) {
        tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);
    }

    tuh_hid_receive_report(dev_addr, instance);
    // if (!tuh_hid_receive_report(dev_addr, instance)) {
    //     ahprintf("[PLUG] warning! report request failed; delayed initialisation?\n");
    // }
}

/**
 * HID disconnection callback
 *
 * @param dev_addr    Address of disconnected device
 * @param instance    Instance of disconnected device
 */
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    uint8_t hid_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (hid_info[instance].desc_report != NULL) {
        free(hid_info[instance].desc_report);
    }

    dbgcons_unplug(hid_protocol_type[hid_protocol]);
}

/**
 * HID report event has occurred
 *
 * @param dev_addr    Address of device sending report
 * @param instance    Instance of device sending report
 * @param report      Address of report structure
 * @param len         Length of report structure
 */
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
    uint8_t const hid_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (hid_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            handle_event_keyboard(dev_addr, instance, (hid_keyboard_report_t const *)report);
            break;

        case HID_ITF_PROTOCOL_MOUSE:
            handle_event_mouse(dev_addr, instance, (hid_mouse_report_t const *)report);
            break;

        default:
            // if report was not immediately identifiable as a keyboard event, read the usage page;
            // some reports have a classifier as "desktop" for media keys, power, or are just encapsulated.
            process_report(dev_addr, instance, report, len);
            break;
    }

    // continue to request to receive report
    tuh_hid_receive_report(dev_addr, instance);
    // if (!tuh_hid_receive_report(dev_addr, instance)) {
    //     ahprintf("[ERROR] unable to receive hid event report\n");
    // }
}

/**
 * HID Boot Protocol keeps a six-key buffer of pressed keys. Return true if keycode is "pressed".
 *
 * @param report    Address of hid_keyboard_report_t struct
 * @param keycode   Keycode to look for
 * @return true     Key is currently pressed
 * @return false    Key is not currently pressed
 */
static inline bool key_pressed(hid_keyboard_report_t const *report, uint8_t keycode)
{
    for (uint8_t pos = 0; pos < 6; pos++)
        if (report->keycode[pos] == keycode)
            return true;

    return false;
}

/**
 * Process incoming event and pass off to device-centric handler.
 *
 * @param dev_addr  Address of reporting device
 * @param instance  Instance of reporting device
 * @param report    Address of the report data structure
 * @param len       Size of the report event
 */
static void process_report(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
    uint8_t const report_count = hid_info[instance].report_count;
    tuh_hid_report_info_t *report_info_arr = hid_info[instance].report_info;
    tuh_hid_report_info_t *report_info = NULL;

    if (report_count == 1 && report_info_arr[0].report_id == 0) {
        // single report with id of 0 is a single-shot report
        report_info = &report_info_arr[0];
    } else {
        // (possibly, but not mandatory) array of reports
        // @todo i don't understand why, if we're facing an array of reports, why we're only going to process the
        // report containing the identifier at byte 0, but maybe one day that will make sense.
        uint8_t const report_id = report[0];

        // locate the array member matching the report ID we are intending to process (then exit the loop)
        for (uint8_t i = 0; i < report_count; i++) {
            if (report_id == report_info_arr[i].report_id) {
                report_info = &report_info_arr[i];
                break;
            }
        }

        report++;
        len--;
    }

    if (!report_info) {
        // report_info wasn't set in the above loop so we cannot continue
        return;
    }

    // process report event based on usage_page class and usage device
    if (report_info->usage_page == HID_USAGE_PAGE_DESKTOP) {
        switch (report_info->usage) {
            case HID_USAGE_DESKTOP_KEYBOARD:
                // keyboard event; let's hope it appears as a boot proto event or else this will break
                handle_event_keyboard(dev_addr, instance, (hid_keyboard_report_t const *) report);
                break;

            case HID_USAGE_DESKTOP_MOUSE:
                // mouse event
                handle_event_mouse(dev_addr, instance, (hid_mouse_report_t const *) report);
                break;

            default:
                break;
        }
    }
}

/**
 * Handle the mouse event sent to us.
 *
 * @param dev_addr  Device address of report
 * @param instance  Instance number of reporting device
 * @param report    Address of hid_mouse_report_t structure of current mouse event
 */
static void handle_event_mouse(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const *report)
{
    static hid_mouse_report_t last_report = { 0 };

    if (report == NULL) {
        return;
    }

    // have buttons changed since the last report?
    if ((report->buttons & MOUSE_BUTTON_LEFT) && !(last_report.buttons & MOUSE_BUTTON_LEFT))
        amiga_quad_mouse_button(AQM_LEFT, true);
    if (!(report->buttons & MOUSE_BUTTON_LEFT) && (last_report.buttons & MOUSE_BUTTON_LEFT))
        amiga_quad_mouse_button(AQM_LEFT, false);

    if ((report->buttons & MOUSE_BUTTON_MIDDLE) && !(last_report.buttons & MOUSE_BUTTON_MIDDLE))
        amiga_quad_mouse_button(AQM_MIDDLE, true);
    if (!(report->buttons & MOUSE_BUTTON_MIDDLE) && (last_report.buttons & MOUSE_BUTTON_MIDDLE))
        amiga_quad_mouse_button(AQM_MIDDLE, false);

    if ((report->buttons & MOUSE_BUTTON_RIGHT) && !(last_report.buttons & MOUSE_BUTTON_RIGHT))
        amiga_quad_mouse_button(AQM_RIGHT, true);
    if (!(report->buttons & MOUSE_BUTTON_RIGHT) && (last_report.buttons & MOUSE_BUTTON_RIGHT))
        amiga_quad_mouse_button(AQM_RIGHT, false);

    if (report->x || report->y)
        amiga_quad_mouse_set_motion(report->x, report->y);

    last_report = *report;
}

static uint8_t led_report = 0;

/**
 * Handle the keyboard event sent to us.
 *
 * @param dev_addr  Device address of report
 * @param instance  Instance number of reporting device
 * @param report    Address of hid_keyboard_report_t structure of current keyboard event (boot proto?)
 */
static void handle_event_keyboard(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report)
{
    // keep hold of older key event reports; init empty keyboard report
    static hid_keyboard_report_t last_report = { 0, 0, {0} };
    uint8_t pos;

    // check to see if a keypress is a new keypress or in the last report
    for (pos = 0; pos < 6; pos++) {
        if (report->keycode[pos] && !key_pressed(&last_report, report->keycode[pos])) {
            // this is a new keypress; pass on to the amiga as a down event
            // @todo right now, menu and right gui are both mapped to right amiga; if one is released, an ramiga up is sent
            // probably something which can be fixed in keyboard_serial_io.c
            amiga_hid_send(report->keycode[pos], false);
        }

        if (last_report.keycode[pos] && !key_pressed(report, last_report.keycode[pos])) {
            // key has been released; send "up" code to amiga
            amiga_hid_send(last_report.keycode[pos], true);
        }
    }

    // check modifier state
    _MULTI_MOD_CHECK(KEYBOARD_MODIFIER_LEFTCTRL, KEYBOARD_MODIFIER_RIGHTCTRL);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTALT);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTALT);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTSHIFT);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTSHIFT);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTGUI);
    // @todo menu key vs right gui thing; see above
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTGUI);

    if (amiga_caps_lock()) {
        if (!(led_report & KEYBOARD_LED_CAPSLOCK)) {
            led_report |= KEYBOARD_LED_CAPSLOCK;

            // ahprintf("[hid] turning caps lock led on\n");
            tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &led_report, 1);
        }
    } else {
        if (led_report & KEYBOARD_LED_CAPSLOCK) {
            led_report &= ~KEYBOARD_LED_CAPSLOCK;

            // ahprintf("[hid] turning caps lock led off\n");
            tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &led_report, 1);
        }
    }

    last_report = *report;
}
