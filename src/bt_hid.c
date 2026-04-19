/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * bluetooth hid host integration for pico w builds.
 */

#include "bt_hid.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "btstack_config.h"
#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/util/queue.h"

#include "input_bridge_bt.h"
#include "util/output.h"

#define BT_HID_QUEUE_DEPTH 64
#define BT_CLASSIC_DESCRIPTOR_STORAGE_SIZE 512
#define BT_HID_LOCAL_NAME "AmigaHID Pico"

#define HID_USAGE_DESKTOP_X     0x30
#define HID_USAGE_DESKTOP_Y     0x31
#define HID_USAGE_DESKTOP_WHEEL 0x38

typedef enum
{
    BT_HID_QUEUE_DISCONNECT = 0,
    BT_HID_QUEUE_KEYBOARD,
    BT_HID_QUEUE_MOUSE,
} bt_hid_queue_type_t;

typedef struct
{
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycode[6];
} bt_hid_keyboard_report_t;

typedef struct
{
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
    int8_t pan;
} bt_hid_mouse_report_t;

typedef struct
{
    uint8_t slot;
    bt_hid_queue_type_t type;
    union
    {
        bt_hid_keyboard_report_t keyboard;
        bt_hid_mouse_report_t mouse;
    };
} bt_hid_queue_entry_t;

typedef struct
{
    bool in_use;
    uint16_t hid_cid;
    hid_protocol_mode_t protocol_mode;
} bt_classic_connection_t;

typedef enum
{
    BT_LE_STATE_OFF = 0,
    BT_LE_STATE_SCANNING,
    BT_LE_STATE_CONNECTING,
    BT_LE_STATE_ENCRYPTING,
    BT_LE_STATE_SERVICE_QUERY,
    BT_LE_STATE_CHARACTERISTIC_QUERY,
    BT_LE_STATE_ENABLE_KEYBOARD,
    BT_LE_STATE_ENABLE_MOUSE,
    BT_LE_STATE_READY,
} bt_le_state_t;

static queue_t bt_hid_queue;
static bool bt_hid_ready;

static bt_classic_connection_t bt_classic_connections[INPUT_BRIDGE_BT_CLASSIC_SLOTS];
static uint8_t bt_classic_descriptor_storage[BT_CLASSIC_DESCRIPTOR_STORAGE_SIZE];
static const hid_protocol_mode_t bt_classic_report_mode = HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT;

static btstack_packet_callback_registration_t bt_classic_hci_event_callback;
static btstack_packet_callback_registration_t bt_le_hci_event_callback;
static btstack_packet_callback_registration_t bt_le_sm_event_callback;

static bt_le_state_t bt_le_state = BT_LE_STATE_OFF;
static bd_addr_t bt_le_addr;
static bd_addr_type_t bt_le_addr_type;
static hci_con_handle_t bt_le_connection_handle = HCI_CON_HANDLE_INVALID;
static gatt_client_service_t bt_le_hid_service;
static gatt_client_characteristic_t bt_le_protocol_mode_characteristic;
static gatt_client_characteristic_t bt_le_boot_keyboard_characteristic;
static gatt_client_characteristic_t bt_le_boot_mouse_characteristic;
static gatt_client_notification_t bt_le_keyboard_notifications;
static gatt_client_notification_t bt_le_mouse_notifications;
static bool bt_le_has_protocol_mode;
static bool bt_le_has_boot_keyboard;
static bool bt_le_has_boot_mouse;

static inline uint8_t bt_classic_input_slot(uint8_t classic_slot)
{
    return (uint8_t)(INPUT_BRIDGE_BT_CLASSIC_SLOT_BASE + classic_slot);
}

static inline uint8_t bt_le_input_slot(void)
{
    return INPUT_BRIDGE_BT_LE_SLOT_BASE;
}

static int8_t bt_hid_clamp_i8(int32_t value)
{
    if (value > INT8_MAX)
        return INT8_MAX;
    if (value < INT8_MIN)
        return INT8_MIN;

    return (int8_t)value;
}

static void bt_hid_enqueue(bt_hid_queue_entry_t const *entry, bool critical)
{
    if (queue_try_add(&bt_hid_queue, entry))
        return;

    if (critical) {
        bt_hid_queue_entry_t dropped;

        if (queue_try_remove(&bt_hid_queue, &dropped) && queue_try_add(&bt_hid_queue, entry))
            return;
    }

    ahprintf("[bt] queue full, dropped event type %u slot %u\n", entry->type, entry->slot);
}

static void bt_hid_enqueue_disconnect(uint8_t slot)
{
    bt_hid_queue_entry_t entry = {
        .slot = slot,
        .type = BT_HID_QUEUE_DISCONNECT,
    };

    bt_hid_enqueue(&entry, true);
}

static void bt_hid_enqueue_keyboard(uint8_t slot, bt_hid_keyboard_report_t const *report)
{
    bt_hid_queue_entry_t entry = {
        .slot = slot,
        .type = BT_HID_QUEUE_KEYBOARD,
    };

    entry.keyboard = *report;
    bt_hid_enqueue(&entry, false);
}

static void bt_hid_enqueue_mouse(uint8_t slot, bt_hid_mouse_report_t const *report)
{
    bt_hid_queue_entry_t entry = {
        .slot = slot,
        .type = BT_HID_QUEUE_MOUSE,
    };

    entry.mouse = *report;
    bt_hid_enqueue(&entry, false);
}

static void bt_hid_append_keycode(bt_hid_keyboard_report_t *report, uint8_t keycode)
{
    for (uint8_t pos = 0; pos < 6; pos++) {
        if (report->keycode[pos] == keycode)
            return;
        if (report->keycode[pos] == 0) {
            report->keycode[pos] = keycode;
            return;
        }
    }
}

static int8_t bt_classic_find_slot(uint16_t hid_cid)
{
    for (uint8_t slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++) {
        if (bt_classic_connections[slot].in_use && (bt_classic_connections[slot].hid_cid == hid_cid))
            return (int8_t)slot;
    }

    return -1;
}

static int8_t bt_classic_find_free_slot(void)
{
    for (uint8_t slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++)
        if (!bt_classic_connections[slot].in_use)
            return (int8_t)slot;

    return -1;
}

static void bt_classic_disconnect_slot(uint8_t slot)
{
    if (!bt_classic_connections[slot].in_use)
        return;

    bt_hid_enqueue_disconnect(bt_classic_input_slot(slot));
    memset(&bt_classic_connections[slot], 0, sizeof(bt_classic_connections[slot]));
}

static void bt_classic_parse_report(uint8_t slot, uint8_t const *report, uint16_t report_len)
{
    bt_classic_connection_t *connection = &bt_classic_connections[slot];
    bt_hid_keyboard_report_t keyboard = { 0, 0, {0} };
    bt_hid_mouse_report_t mouse = { 0 };
    btstack_hid_parser_t parser;
    const uint8_t *descriptor;
    uint16_t descriptor_len;
    bool saw_keyboard = false;
    bool saw_mouse = false;

    if ((report == NULL) || (report_len < 2) || (report[0] != 0xa1))
        return;

    report++;
    report_len--;

    if (connection->protocol_mode == HID_PROTOCOL_MODE_BOOT) {
        descriptor = btstack_hid_get_boot_descriptor_data();
        descriptor_len = btstack_hid_get_boot_descriptor_len();
    } else {
        descriptor = hid_descriptor_storage_get_descriptor_data(connection->hid_cid);
        descriptor_len = hid_descriptor_storage_get_descriptor_len(connection->hid_cid);

        if ((descriptor == NULL) || (descriptor_len == 0))
            return;
    }

    btstack_hid_parser_init(&parser, descriptor, descriptor_len, HID_REPORT_TYPE_INPUT, report, report_len);

    while (btstack_hid_parser_has_more(&parser)) {
        uint16_t usage_page;
        uint16_t usage;
        int32_t value;

        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

        switch (usage_page) {
            case HID_USAGE_PAGE_KEYBOARD:
                saw_keyboard = true;

                if ((usage >= HID_USAGE_KEY_KEYBOARD_LEFTCONTROL) && (usage <= HID_USAGE_KEY_KEYBOARD_RIGHT_GUI)) {
                    if (value)
                        keyboard.modifier |= (uint8_t)(1u << (usage - HID_USAGE_KEY_KEYBOARD_LEFTCONTROL));
                    break;
                }

                if ((usage != HID_USAGE_KEY_RESERVED) && (usage < 0x100))
                    bt_hid_append_keycode(&keyboard, (uint8_t)usage);
                break;

            case HID_USAGE_PAGE_BUTTON:
                if ((usage >= 1) && (usage <= 8)) {
                    saw_mouse = true;
                    if (value)
                        mouse.buttons |= (uint8_t)(1u << (usage - 1u));
                }
                break;

            case HID_USAGE_PAGE_DESKTOP:
                switch (usage) {
                    case HID_USAGE_DESKTOP_X:
                        saw_mouse = true;
                        mouse.x = bt_hid_clamp_i8(value);
                        break;

                    case HID_USAGE_DESKTOP_Y:
                        saw_mouse = true;
                        mouse.y = bt_hid_clamp_i8(value);
                        break;

                    case HID_USAGE_DESKTOP_WHEEL:
                        saw_mouse = true;
                        mouse.wheel = bt_hid_clamp_i8(value);
                        break;

                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }

    if (saw_keyboard)
        bt_hid_enqueue_keyboard(bt_classic_input_slot(slot), &keyboard);

    if (saw_mouse)
        bt_hid_enqueue_mouse(bt_classic_input_slot(slot), &mouse);
}

static void bt_classic_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_PIN_CODE_REQUEST: {
            bd_addr_t event_addr;

            hci_event_pin_code_request_get_bd_addr(packet, event_addr);
            gap_pin_code_response(event_addr, "0000");
            break;
        }

        case HCI_EVENT_USER_CONFIRMATION_REQUEST: {
            bd_addr_t event_addr;

            hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
            gap_ssp_confirmation_response(event_addr);
            break;
        }

        case HCI_EVENT_HID_META:
            switch (hci_event_hid_meta_get_subevent_code(packet)) {
                case HID_SUBEVENT_INCOMING_CONNECTION:
                    if (bt_classic_find_free_slot() >= 0)
                        hid_host_accept_connection(hid_subevent_incoming_connection_get_hid_cid(packet), bt_classic_report_mode);
                    else
                        hid_host_decline_connection(hid_subevent_incoming_connection_get_hid_cid(packet));
                    break;

                case HID_SUBEVENT_CONNECTION_OPENED: {
                    uint8_t status = hid_subevent_connection_opened_get_status(packet);
                    uint16_t hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                    int8_t slot;

                    if (status != ERROR_CODE_SUCCESS) {
                        ahprintf("[bt] classic hid connect failed: 0x%02x\n", status);
                        break;
                    }

                    slot = bt_classic_find_free_slot();
                    if (slot < 0) {
                        hid_host_disconnect(hid_cid);
                        break;
                    }

                    bt_classic_connections[slot].in_use = true;
                    bt_classic_connections[slot].hid_cid = hid_cid;
                    bt_classic_connections[slot].protocol_mode = HID_PROTOCOL_MODE_REPORT;
                    break;
                }

                case HID_SUBEVENT_SET_PROTOCOL_RESPONSE: {
                    int8_t slot = bt_classic_find_slot(hid_subevent_set_protocol_response_get_hid_cid(packet));

                    if ((slot >= 0)
                        && (hid_subevent_set_protocol_response_get_handshake_status(packet) == HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL)) {
                        bt_classic_connections[slot].protocol_mode =
                            (hid_protocol_mode_t)hid_subevent_set_protocol_response_get_protocol_mode(packet);
                    }
                    break;
                }

                case HID_SUBEVENT_REPORT: {
                    int8_t slot = bt_classic_find_slot(hid_subevent_report_get_hid_cid(packet));

                    if (slot >= 0)
                        bt_classic_parse_report((uint8_t)slot, hid_subevent_report_get_report(packet),
                            hid_subevent_report_get_report_len(packet));
                    break;
                }

                case HID_SUBEVENT_CONNECTION_CLOSED: {
                    int8_t slot = bt_classic_find_slot(hid_subevent_connection_closed_get_hid_cid(packet));

                    if (slot >= 0)
                        bt_classic_disconnect_slot((uint8_t)slot);
                    break;
                }

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void bt_le_clear_characteristics(void)
{
    memset(&bt_le_hid_service, 0, sizeof(bt_le_hid_service));
    memset(&bt_le_protocol_mode_characteristic, 0, sizeof(bt_le_protocol_mode_characteristic));
    memset(&bt_le_boot_keyboard_characteristic, 0, sizeof(bt_le_boot_keyboard_characteristic));
    memset(&bt_le_boot_mouse_characteristic, 0, sizeof(bt_le_boot_mouse_characteristic));
    memset(&bt_le_keyboard_notifications, 0, sizeof(bt_le_keyboard_notifications));
    memset(&bt_le_mouse_notifications, 0, sizeof(bt_le_mouse_notifications));
    bt_le_has_protocol_mode = false;
    bt_le_has_boot_keyboard = false;
    bt_le_has_boot_mouse = false;
}

static bool bt_le_adv_event_contains_hid_service(uint8_t const *packet)
{
    uint8_t ad_len = gap_event_advertising_report_get_data_length(packet);
    uint8_t const *ad_data = gap_event_advertising_report_get_data(packet);

    return ad_data_contains_uuid16(ad_len, ad_data, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
}

static void bt_le_start_scan(void)
{
    if (!bt_hid_ready || (bt_le_connection_handle != HCI_CON_HANDLE_INVALID))
        return;

    bt_le_clear_characteristics();
    bt_le_state = BT_LE_STATE_SCANNING;
    gap_set_scan_parameters(0, 48, 48);
    gap_start_scan();
}

static void bt_le_restart_scan(void)
{
    bt_le_connection_handle = HCI_CON_HANDLE_INVALID;
    bt_le_state = BT_LE_STATE_OFF;
    bt_le_start_scan();
}

static void bt_le_disconnect_and_restart(void)
{
    if (bt_le_connection_handle != HCI_CON_HANDLE_INVALID)
        gap_disconnect(bt_le_connection_handle);
    else
        bt_le_restart_scan();
}

static void bt_le_ready(void)
{
    uint8_t boot_protocol_mode = 0;

    if (bt_le_has_protocol_mode) {
        gatt_client_write_value_of_characteristic_without_response(bt_le_connection_handle,
            bt_le_protocol_mode_characteristic.value_handle, 1, &boot_protocol_mode);
    }

    bt_le_state = BT_LE_STATE_READY;
}

static void bt_le_keyboard_notification_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    bt_hid_keyboard_report_t report = { 0, 0, {0} };

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION)
        return;

    if (gatt_event_notification_get_value_length(packet) > sizeof(report))
        memcpy(&report, gatt_event_notification_get_value(packet), sizeof(report));
    else
        memcpy(&report, gatt_event_notification_get_value(packet), gatt_event_notification_get_value_length(packet));

    bt_hid_enqueue_keyboard(bt_le_input_slot(), &report);
}

static void bt_le_mouse_notification_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    bt_hid_mouse_report_t report = { 0 };
    uint8_t const *value;
    uint16_t value_len;

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION)
        return;

    value = gatt_event_notification_get_value(packet);
    value_len = gatt_event_notification_get_value_length(packet);

    if (value_len > 0)
        report.buttons = value[0];
    if (value_len > 1)
        report.x = (int8_t)value[1];
    if (value_len > 2)
        report.y = (int8_t)value[2];
    if (value_len > 3)
        report.wheel = (int8_t)value[3];

    bt_hid_enqueue_mouse(bt_le_input_slot(), &report);
}

static void bt_le_gatt_client_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    gatt_client_characteristic_t characteristic;

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (bt_le_state) {
        case BT_LE_STATE_SERVICE_QUERY:
            switch (hci_event_packet_get_type(packet)) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    gatt_event_service_query_result_get_service(packet, &bt_le_hid_service);
                    break;

                case GATT_EVENT_QUERY_COMPLETE:
                    if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS) {
                        bt_le_disconnect_and_restart();
                        break;
                    }

                    bt_le_state = BT_LE_STATE_CHARACTERISTIC_QUERY;
                    gatt_client_discover_characteristics_for_service(&bt_le_gatt_client_handler,
                        bt_le_connection_handle, &bt_le_hid_service);
                    break;

                default:
                    break;
            }
            break;

        case BT_LE_STATE_CHARACTERISTIC_QUERY:
            switch (hci_event_packet_get_type(packet)) {
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);

                    switch (characteristic.uuid16) {
                        case ORG_BLUETOOTH_CHARACTERISTIC_PROTOCOL_MODE:
                            bt_le_protocol_mode_characteristic = characteristic;
                            bt_le_has_protocol_mode = true;
                            break;

                        case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT:
                            bt_le_boot_keyboard_characteristic = characteristic;
                            bt_le_has_boot_keyboard = true;
                            break;

                        case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT:
                            bt_le_boot_mouse_characteristic = characteristic;
                            bt_le_has_boot_mouse = true;
                            break;

                        default:
                            break;
                    }
                    break;

                case GATT_EVENT_QUERY_COMPLETE:
                    if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS) {
                        bt_le_disconnect_and_restart();
                        break;
                    }

                    if (bt_le_has_boot_keyboard) {
                        bt_le_state = BT_LE_STATE_ENABLE_KEYBOARD;
                        gatt_client_write_client_characteristic_configuration(&bt_le_gatt_client_handler,
                            bt_le_connection_handle, &bt_le_boot_keyboard_characteristic,
                            GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    } else if (bt_le_has_boot_mouse) {
                        bt_le_state = BT_LE_STATE_ENABLE_MOUSE;
                        gatt_client_write_client_characteristic_configuration(&bt_le_gatt_client_handler,
                            bt_le_connection_handle, &bt_le_boot_mouse_characteristic,
                            GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    } else {
                        bt_le_disconnect_and_restart();
                    }
                    break;

                default:
                    break;
            }
            break;

        case BT_LE_STATE_ENABLE_KEYBOARD:
            if (hci_event_packet_get_type(packet) != GATT_EVENT_QUERY_COMPLETE)
                break;

            if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS) {
                bt_le_disconnect_and_restart();
                break;
            }

            gatt_client_listen_for_characteristic_value_updates(&bt_le_keyboard_notifications,
                &bt_le_keyboard_notification_handler, bt_le_connection_handle, &bt_le_boot_keyboard_characteristic);

            if (bt_le_has_boot_mouse) {
                bt_le_state = BT_LE_STATE_ENABLE_MOUSE;
                gatt_client_write_client_characteristic_configuration(&bt_le_gatt_client_handler,
                    bt_le_connection_handle, &bt_le_boot_mouse_characteristic,
                    GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
            } else {
                bt_le_ready();
            }
            break;

        case BT_LE_STATE_ENABLE_MOUSE:
            if (hci_event_packet_get_type(packet) != GATT_EVENT_QUERY_COMPLETE)
                break;

            if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS) {
                bt_le_disconnect_and_restart();
                break;
            }

            gatt_client_listen_for_characteristic_value_updates(&bt_le_mouse_notifications,
                &bt_le_mouse_notification_handler, bt_le_connection_handle, &bt_le_boot_mouse_characteristic);
            bt_le_ready();
            break;

        default:
            break;
    }
}

static void bt_le_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING)
                bt_le_start_scan();
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            if (bt_le_state != BT_LE_STATE_SCANNING)
                break;
            if (!bt_le_adv_event_contains_hid_service(packet))
                break;

            gap_stop_scan();
            gap_event_advertising_report_get_address(packet, bt_le_addr);
            bt_le_addr_type = gap_event_advertising_report_get_address_type(packet);
            bt_le_state = BT_LE_STATE_CONNECTING;
            gap_connect(bt_le_addr, bt_le_addr_type);
            break;

        case HCI_EVENT_META_GAP:
            if (hci_event_gap_meta_get_subevent_code(packet) != GAP_SUBEVENT_LE_CONNECTION_COMPLETE)
                break;

            if (bt_le_state != BT_LE_STATE_CONNECTING)
                break;

            if (gap_subevent_le_connection_complete_get_status(packet) != ERROR_CODE_SUCCESS) {
                bt_le_restart_scan();
                break;
            }

            bt_le_connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
            bt_le_state = BT_LE_STATE_ENCRYPTING;
            sm_request_pairing(bt_le_connection_handle);
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (hci_event_disconnection_complete_get_connection_handle(packet) != bt_le_connection_handle)
                break;

            bt_hid_enqueue_disconnect(bt_le_input_slot());
            bt_le_clear_characteristics();
            bt_le_restart_scan();
            break;

        default:
            break;
    }
}

static void bt_le_sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    bool connect_to_service = false;

    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;

        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            ahprintf("[btle] confirm %lu\n", (unsigned long)sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_numeric_comparison_request_get_handle(packet));
            break;

        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            ahprintf("[btle] passkey %lu\n", (unsigned long)sm_event_passkey_display_number_get_passkey(packet));
            break;

        case SM_EVENT_PAIRING_COMPLETE:
            if (sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS)
                connect_to_service = true;
            else
                bt_le_disconnect_and_restart();
            break;

        case SM_EVENT_REENCRYPTION_COMPLETE:
            connect_to_service = true;
            break;

        default:
            break;
    }

    if (!connect_to_service || (bt_le_connection_handle == HCI_CON_HANDLE_INVALID))
        return;

    bt_le_state = BT_LE_STATE_SERVICE_QUERY;
    gatt_client_discover_primary_services_by_uuid16(&bt_le_gatt_client_handler, bt_le_connection_handle,
        ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
}

void bt_hid_init(void)
{
    if (bt_hid_ready)
        return;

    for (uint8_t slot = INPUT_BRIDGE_BT_CLASSIC_SLOT_BASE; slot < INPUT_BRIDGE_MAX_SLOTS; slot++)
        input_bridge_reset(slot);

    queue_init(&bt_hid_queue, sizeof(bt_hid_queue_entry_t), BT_HID_QUEUE_DEPTH);

    if (cyw43_arch_init()) {
        ahprintf("[bt] cyw43 init failed\n");
        return;
    }

    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    gatt_client_init();

    hid_host_init(bt_classic_descriptor_storage, sizeof(bt_classic_descriptor_storage));
    hid_host_register_packet_handler(&bt_classic_packet_handler);

    gap_set_local_name(BT_HID_LOCAL_NAME);
    gap_discoverable_control(1);
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    gap_ssp_set_auto_accept(1);

    bt_classic_hci_event_callback.callback = &bt_classic_packet_handler;
    hci_add_event_handler(&bt_classic_hci_event_callback);

    bt_le_hci_event_callback.callback = &bt_le_packet_handler;
    hci_add_event_handler(&bt_le_hci_event_callback);

    bt_le_sm_event_callback.callback = &bt_le_sm_packet_handler;
    sm_add_event_handler(&bt_le_sm_event_callback);

    bt_le_clear_characteristics();
    bt_hid_ready = true;

    hci_power_control(HCI_POWER_ON);
}

void bt_hid_task(void)
{
    bt_hid_queue_entry_t entry;

    if (!bt_hid_ready)
        return;

    while (queue_try_remove(&bt_hid_queue, &entry)) {
        switch (entry.type) {
            case BT_HID_QUEUE_DISCONNECT:
                input_bridge_disconnect(entry.slot);
                break;

            case BT_HID_QUEUE_KEYBOARD:
                input_bridge_handle_keyboard_boot(entry.slot, entry.keyboard.modifier, entry.keyboard.keycode);
                break;

            case BT_HID_QUEUE_MOUSE:
                input_bridge_handle_mouse_boot(entry.slot, entry.mouse.buttons, entry.mouse.x, entry.mouse.y,
                    entry.mouse.wheel);
                break;
        }
    }
}
