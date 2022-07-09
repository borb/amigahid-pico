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
 */

// these reside within the tinyusb sdk and are not part of this project source
#include "bsp/board.h"
#include "tusb.h"

// other includes
#include <stdint.h>

#include "tusb_config.h"
#include "platform/amiga/keyboard_serial_io.h"  // amiga only, for now, until i get hold of an ST :D
#include "platform/amiga/keyboard.h"
#include "platform/amiga/quad_mouse.h"
#include "util/output.h"
#include "util/debug_cons.h"

// maximum number of reports per hid device
#define MAX_REPORT 4

// textual representations of attached devices
const uint8_t hid_protocol_type[] = { NULL, AP_H_KEYBOARD, AP_H_MOUSE };

// hid information structure
static struct _hid_info
{
    uint8_t report_count;
    tuh_hid_report_info_t report_info[MAX_REPORT];
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
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
    uint8_t hid_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    dbgcons_plug(hid_protocol_type[hid_protocol]);

    // this part doesn't entirely make sense to me; hid devices come in two modes, boot protocol and report;
    // as i understand it, boot proto is intended for simplistic software such as bios which don't want to
    // implement a full stack. so if we're not in boot proto mode, display... something?
    // this might be number of interfaces on a device (think wireless kbd+mouse receiver). maybe. speculation.
    if (hid_protocol == HID_ITF_PROTOCOL_NONE) {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
        // ahprintf("[PLUG] %02x report(s)\n", hid_info[instance].report_count);
    }

    if (!tuh_hid_receive_report(dev_addr, instance)) {
        // ahprintf("[PLUG] warning! report request failed; delayed initialisation?\n");
    }
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
    // if (!tuh_hid_receive_report(dev_addr, instance))
        // ahprintf("[ERROR] unable to receive hid event report\n");
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
        // ahprintf("[ERROR] report_info pointer was not set during process_report(), value: $%x, report_count was %d\n", report_info, report_count);
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
        // ahprintf("[hid] report was null, aborting mouse event\n");
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

    // this would spam horrendously, so even when debug messages are on, this is probably... too much.
    // ahprintf("[hid] x: %d y: %d\n", report->x, report->y);

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
#ifndef MENU_IS_RAMIGA
            // ignore menu
            if (report->keycode[pos] == HID_KEY_APPLICATION)
                continue;
#endif

            ahprintf("[hid] sending key down (amiga: %02x, hid: %02x)\n", mapHidToAmiga[report->keycode[pos]], report->keycode[pos]);
            amiga_send(mapHidToAmiga[report->keycode[pos]], false);
        }

        if (last_report.keycode[pos] && !key_pressed(report, last_report.keycode[pos])) {
            // key has been released; send "up" code to amiga
#ifndef MENU_IS_RAMIGA
            // ignore menu
            if (last_report.keycode[pos] == HID_KEY_APPLICATION)
                continue;
#endif
            amiga_send(mapHidToAmiga[last_report.keycode[pos]], true);
        }
    }

    // check modifier state
    if (((report->modifier & KEYBOARD_MODIFIER_LEFTCTRL) && !(last_report.modifier & KEYBOARD_MODIFIER_LEFTCTRL))
        || ((report->modifier & KEYBOARD_MODIFIER_RIGHTCTRL) && !(last_report.modifier & KEYBOARD_MODIFIER_RIGHTCTRL))
    ) {
        ahprintf("[hid] ctrl pressed, sending ctrl down\n");
        amiga_send(AMIGA_CTRL, false);
    }
    if ((!(report->modifier & KEYBOARD_MODIFIER_LEFTCTRL) && (last_report.modifier & KEYBOARD_MODIFIER_LEFTCTRL))
        || (!(report->modifier & KEYBOARD_MODIFIER_RIGHTCTRL) && (last_report.modifier & KEYBOARD_MODIFIER_RIGHTCTRL))
    ) {
        ahprintf("[hid] ctrl released, sending ctrl up\n");
        amiga_send(AMIGA_CTRL, true);
    }

    if ((report->modifier & KEYBOARD_MODIFIER_LEFTALT) && !(last_report.modifier & KEYBOARD_MODIFIER_LEFTALT)) {
        ahprintf("[hid] left alt pressed, sending left alt down\n");
        amiga_send(AMIGA_LALT, false);
    }
    if (!(report->modifier & KEYBOARD_MODIFIER_LEFTALT) && (last_report.modifier & KEYBOARD_MODIFIER_LEFTALT)) {
        ahprintf("[hid] left alt released, sending left alt up\n");
        amiga_send(AMIGA_LALT, true);
    }

    if ((report->modifier & KEYBOARD_MODIFIER_RIGHTALT) && !(last_report.modifier & KEYBOARD_MODIFIER_RIGHTALT)) {
        ahprintf("[hid] right alt pressed, sending right alt down\n");
        amiga_send(AMIGA_RALT, false);
    }
    if (!(report->modifier & KEYBOARD_MODIFIER_RIGHTALT) && (last_report.modifier & KEYBOARD_MODIFIER_RIGHTALT)) {
        ahprintf("[hid] right alt released, sending right alt up\n");
        amiga_send(AMIGA_RALT, true);
    }

    if ((report->modifier & KEYBOARD_MODIFIER_LEFTSHIFT) && !(last_report.modifier & KEYBOARD_MODIFIER_LEFTSHIFT)) {
        ahprintf("[hid] left shift pressed, sending left shift down\n");
        amiga_send(AMIGA_LSHIFT, false);
    }
    if (!(report->modifier & KEYBOARD_MODIFIER_LEFTSHIFT) && (last_report.modifier & KEYBOARD_MODIFIER_LEFTSHIFT)) {
        ahprintf("[hid] left shift released, sending left shift up\n");
        amiga_send(AMIGA_LSHIFT, true);
    }

    if ((report->modifier & KEYBOARD_MODIFIER_RIGHTSHIFT) && !(last_report.modifier & KEYBOARD_MODIFIER_RIGHTSHIFT)) {
        ahprintf("[hid] right shift pressed, sending right shift down\n");
        amiga_send(AMIGA_RSHIFT, false);
    }
    if (!(report->modifier & KEYBOARD_MODIFIER_RIGHTSHIFT) && (last_report.modifier & KEYBOARD_MODIFIER_RIGHTSHIFT)) {
        ahprintf("[hid] right shift released, sending right shift up\n");
        amiga_send(AMIGA_RSHIFT, true);
    }

    if ((report->modifier & KEYBOARD_MODIFIER_LEFTGUI) && !(last_report.modifier & KEYBOARD_MODIFIER_LEFTGUI)) {
        ahprintf("[hid] left gui pressed, sending left amiga down\n");
        amiga_send(AMIGA_LAMIGA, false);
    }
    if (!(report->modifier & KEYBOARD_MODIFIER_LEFTGUI) && (last_report.modifier & KEYBOARD_MODIFIER_LEFTGUI)) {
        ahprintf("[hid] left gui released, sending left amiga up\n");
        amiga_send(AMIGA_LAMIGA, true);
    }

#ifndef MENU_IS_RAMIGA
    if ((report->modifier & KEYBOARD_MODIFIER_RIGHTGUI) && !(last_report.modifier & KEYBOARD_MODIFIER_RIGHTGUI)) {
        ahprintf("[hid] right gui pressed, sending right amiga down\n");
        amiga_send(AMIGA_RAMIGA, false);
    }
    if (!(report->modifier & KEYBOARD_MODIFIER_RIGHTGUI) && (last_report.modifier & KEYBOARD_MODIFIER_RIGHTGUI)) {
        ahprintf("[hid] right gui released, sending right amiga up\n");
        amiga_send(AMIGA_RAMIGA, true);
    }
#endif

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
