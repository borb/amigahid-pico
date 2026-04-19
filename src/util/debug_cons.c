/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * debug console routines.
 */

#include <stdint.h>
#include <stdio.h>

#include "debug_cons.h"
#include "display/disp_ssd.h"
#include "output.h"

#define DBGCONS_OLED_COLS 21

struct
{
    uint8_t hid_keyboard, hid_mouse, hid_controller;
    uint8_t plug_events, unplug_events;
} debug_counters;

#ifdef ENABLE_BLUETOOTH_HID
static bool bt_passkey_active;

static void dbgcons_write_padded_line(uint8_t line, char const *message)
{
    char linebuf[DBGCONS_OLED_COLS + 1] = "";

    snprintf(linebuf, sizeof(linebuf), "%-*.*s", DBGCONS_OLED_COLS, DBGCONS_OLED_COLS,
        message != NULL ? message : "");
    disp_write(0, line, linebuf);
}
#endif

void dbgcons_init()
{
    ahprintf(
        VT_ED_CLS "amigahid-pico by nine <nine@aphlor.org>, https://github.com/borb/amigahid-pico"
    );

    debug_counters.hid_keyboard = 0;
    debug_counters.hid_mouse = 0;
    debug_counters.hid_controller = 0;
    debug_counters.plug_events = 0;
    debug_counters.unplug_events = 0;

    dbgcons_print_counters();
#ifdef ENABLE_BLUETOOTH_HID
    dbgcons_bt_status("bt off");
#endif
#ifdef DEBUG_HID_STATUS
    disp_write(0, 2, "hid --");
#endif
#ifdef ENABLE_BLUETOOTH_HID
    dbgcons_bt_passkey_clear();
#elif defined(DEBUG_MOUSE)
    disp_write(0, 3, "mouse --");
#endif
}

void dbgcons_print_counters()
{
    char linebuf[32] = "";

    ahprintf(
        VT_CUP_POS VT_EL_LIN
        "[system] key: %02x mouse: %02x joy: %02x total plug: %02x total unplug: %02x\n",
        3, 1,
        debug_counters.hid_keyboard,
        debug_counters.hid_mouse,
        debug_counters.hid_controller,
        debug_counters.plug_events,
        debug_counters.unplug_events
    );

    sprintf(
        linebuf,
        "usb    k:%02x m:%02x j:%02x",
        debug_counters.hid_keyboard,
        debug_counters.hid_mouse,
        debug_counters.hid_controller
    );

    disp_write(0, 0, linebuf);
}

void dbgcons_plug(enum debug_plug_types devtype)
{
    switch (devtype) {
        case AP_H_KEYBOARD:
            debug_counters.hid_keyboard++;
            break;
        case AP_H_MOUSE:
            debug_counters.hid_mouse++;
            break;
        case AP_H_CONTROLLER:
            debug_counters.hid_controller++;
            break;
        case AP_H_UNKNOWN:
        default:
            break;
    }
    debug_counters.plug_events++;
    dbgcons_print_counters();
}

void dbgcons_unplug(enum debug_plug_types devtype)
{
    switch (devtype) {
        case AP_H_KEYBOARD:
            debug_counters.hid_keyboard--;
            break;
        case AP_H_MOUSE:
            debug_counters.hid_mouse--;
            break;
        case AP_H_CONTROLLER:
            debug_counters.hid_controller--;
            break;
        case AP_H_UNKNOWN:
        default:
            break;
    }
    debug_counters.unplug_events++;
    dbgcons_print_counters();
}

void dbgcons_amiga_key(uint8_t incode, uint8_t outcode, char *updown)
{
#ifdef DEBUG_KEYBOARD
    char linebuf[32] = "";

    ahprintf(
        VT_CUP_POS VT_EL_LIN
        "[amigak] hid in: %02x amiga out: %02x up/down: %s\n",
        4, 1,
        incode, outcode, updown
    );

    sprintf(
        linebuf,
        "amikb hid:%02x ami:%02x %s",
        incode, outcode, updown
    );

#ifndef ENABLE_BLUETOOTH_HID
    disp_write(0, 1, linebuf);
#endif
#else
    (void)incode;
    (void)outcode;
    (void)updown;
#endif
}

void dbgcons_amiga_mod(uint8_t outcode, char updown)
{
    // ls rs cl ct la ra lam ram
}

void dbgcons_hid_status(uint8_t dev_addr, uint8_t instance, uint8_t hid_protocol, bool receive_ok, uint8_t report_count, bool mounted)
{
#ifdef DEBUG_HID_STATUS
    char linebuf[32] = "";

    snprintf(
        linebuf,
        sizeof(linebuf),
        "hid %c a%02x i%02x p%02x r%u",
        mounted ? '+' : '-',
        dev_addr,
        instance,
        hid_protocol,
        report_count
    );

    ahprintf(
        VT_CUP_POS VT_EL_LIN
        "[hid] %s addr:%02x inst:%02x proto:%02x reports:%u rx:%s\n",
        5, 1,
        mounted ? "mount" : "umount",
        dev_addr,
        instance,
        hid_protocol,
        report_count,
        receive_ok ? "ok" : "fail"
    );

    disp_write(0, 2, linebuf);

    snprintf(
        linebuf,
        sizeof(linebuf),
        "rx %s",
        receive_ok ? "ok" : "fail"
    );
    disp_write(17, 2, linebuf);
#else
    (void)dev_addr;
    (void)instance;
    (void)hid_protocol;
    (void)receive_ok;
    (void)report_count;
    (void)mounted;
#endif
}

void dbgcons_mouse_report(int16_t x, int16_t y, uint8_t buttons)
{
#ifdef DEBUG_MOUSE
    char linebuf[32] = "";

    snprintf(
        linebuf,
        sizeof(linebuf),
        "m x%+04d y%+04d b%02x",
        x,
        y,
        buttons
    );

    ahprintf(
        VT_CUP_POS VT_EL_LIN
        "[mouse] x:%d y:%d buttons:%02x\n",
        6, 1,
        x,
        y,
        buttons
    );

#ifdef ENABLE_BLUETOOTH_HID
    if (!bt_passkey_active)
        disp_write(0, 3, linebuf);
#else
    disp_write(0, 3, linebuf);
#endif
#else
    (void)x;
    (void)y;
    (void)buttons;
#endif
}

void dbgcons_bt_status(char const *status)
{
#ifdef ENABLE_BLUETOOTH_HID
    dbgcons_write_padded_line(1, status);
#else
    (void)status;
#endif
}

void dbgcons_bt_passkey(char const *message)
{
#ifdef ENABLE_BLUETOOTH_HID
    bt_passkey_active = true;
    dbgcons_write_padded_line(3, message);
#else
    (void)message;
#endif
}

void dbgcons_bt_passkey_clear(void)
{
#ifdef ENABLE_BLUETOOTH_HID
    bt_passkey_active = false;
#ifdef DEBUG_MOUSE
    dbgcons_write_padded_line(3, "mouse --");
#else
    dbgcons_write_padded_line(3, "");
#endif
#endif
}
