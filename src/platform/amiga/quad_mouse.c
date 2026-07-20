/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga quadrature mouse interface.
 */

#include "config.h"
#include "quad_mouse.h"
#include "util/debug_cons.h"
#include "util/output.h"

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"

#define AQM_MOTION_QUEUE_DEPTH 32
#define AQM_WHEEL_QUEUE_DEPTH 16
#define AQM_STEP_INTERVAL_US 300
#define AQM_WHEEL_STEP_US 4
#define AQM_WHEEL_HOLD_US 40
#define AQM_WHEEL_RESPONSE_TIMEOUT_US 2000
#define AQM_TANKMOUSE_WHEEL_UP 0x0a
#define AQM_TANKMOUSE_WHEEL_DOWN 0x09

typedef struct
{
    int16_t x;
    int16_t y;
} aqm_motion_t;

typedef struct
{
    uint8_t steps;
    int8_t direction;
} aqm_axis_move_t;

static queue_t motion_queue;
static queue_t wheel_queue;
static volatile bool button_pressed[3];
static volatile uint16_t wheel_queued;
static volatile uint16_t wheel_requests;
static volatile uint16_t wheel_responses;
volatile uint8_t motion_divider = 2;

enum _mouse_pin_state { LOW, HIGH };

static inline int16_t _aqm_add_clamped(int16_t value, int16_t delta)
{
    int32_t sum = value + delta;

    if (sum > INT16_MAX)
        sum = INT16_MAX;
    else if (sum < INT16_MIN)
        sum = INT16_MIN;

    return (int16_t)sum;
}

static inline void _aqm_gpio_set(uint gpio, enum _mouse_pin_state state)
{
    if (state == LOW) {
        gpio_put(gpio, 0);
        gpio_set_dir(gpio, GPIO_OUT);
        return;
    }

    // assume it's high otherwise
    gpio_set_dir(gpio, GPIO_IN);
}

static inline bool _aqm_gpio_active(uint gpio)
{
    return !gpio_get(gpio);
}

static inline void _aqm_set_quad_state(uint gpio_main, uint gpio_quad, uint8_t state)
{
    switch (state & 3u) {
        case 0:
            _aqm_gpio_set(gpio_main, HIGH);
            _aqm_gpio_set(gpio_quad, LOW);
            break;
        case 1:
            _aqm_gpio_set(gpio_main, HIGH);
            _aqm_gpio_set(gpio_quad, HIGH);
            break;
        case 2:
            _aqm_gpio_set(gpio_main, LOW);
            _aqm_gpio_set(gpio_quad, HIGH);
            break;
        case 3:
            _aqm_gpio_set(gpio_main, LOW);
            _aqm_gpio_set(gpio_quad, LOW);
            break;
    }
}

static inline void _aqm_step_quad_axis(
    uint gpio_main,
    uint gpio_quad,
    uint8_t *state,
    uint8_t *phase,
    int8_t direction)
{
    if (direction < 0) {
        (*state)--;
        (*phase)--;
    } else {
        (*state)++;
        (*phase)++;
    }

    *state &= 3u;
    *phase &= 3u;
    _aqm_set_quad_state(gpio_main, gpio_quad, *state);
}

/*
 * TankMouse/Cocolino reads the low two bits of each Amiga mouse counter and
 * Gray-decodes them. Convert one 2-bit protocol axis value back to the counter
 * phase the driver expects to sample.
 */
static uint8_t _aqm_tankmouse_axis_code_to_phase(uint8_t code_axis)
{
    uint8_t code_bit0 = code_axis & 1u;
    uint8_t code_bit1 = (code_axis >> 1) & 1u;

    return ((code_bit0 ^ code_bit1) | (code_bit1 << 1)) & 3u;
}

static aqm_axis_move_t _aqm_step_quad_axis_to(
    uint gpio_main,
    uint gpio_quad,
    uint8_t *state,
    uint8_t *phase,
    uint8_t target)
{
    uint8_t forward = (target - *phase) & 3u;
    uint8_t backward = (*phase - target) & 3u;
    aqm_axis_move_t move = { 0, 1 };

    if (backward < forward) {
        move.steps = backward;
        move.direction = -1;
    } else {
        move.steps = forward;
        move.direction = 1;
    }

    for (uint8_t i = 0; i < move.steps; i++) {
        _aqm_step_quad_axis(gpio_main, gpio_quad, state, phase, move.direction);
        busy_wait_us_32(AQM_WHEEL_STEP_US);
    }

    return move;
}

static void _aqm_undo_quad_axis_move(
    uint gpio_main,
    uint gpio_quad,
    uint8_t *state,
    uint8_t *phase,
    aqm_axis_move_t move)
{
    for (uint8_t i = 0; i < move.steps; i++) {
        _aqm_step_quad_axis(gpio_main, gpio_quad, state, phase, -move.direction);
        busy_wait_us_32(AQM_WHEEL_STEP_US);
    }
}

static bool _aqm_wheel_dequeue(uint8_t *code)
{
    return queue_try_remove(&wheel_queue, code);
}

static void _aqm_wheel_clear(void)
{
    uint8_t dropped;

    while (queue_try_remove(&wheel_queue, &dropped)) {
    }
}

static void _aqm_wheel_enqueue(uint8_t code)
{
    if (!queue_try_add(&wheel_queue, &code)) {
        uint8_t dropped;

        if (queue_try_remove(&wheel_queue, &dropped))
            queue_try_add(&wheel_queue, &code);
    }

    wheel_queued++;
}

static void _aqm_tankmouse_respond(
    uint8_t code,
    uint8_t *quad_mx_state,
    uint8_t *quad_my_state,
    uint8_t *quad_mx_phase,
    uint8_t *quad_my_phase)
{
    uint8_t target_x = _aqm_tankmouse_axis_code_to_phase(code & 3u);
    uint8_t target_y = _aqm_tankmouse_axis_code_to_phase((code >> 2) & 3u);
    aqm_axis_move_t x_move;
    aqm_axis_move_t y_move;
    uint32_t started_at;

    // The TankMouse driver treats left-button high as "no middle button edge".
    _aqm_gpio_set(QM1_AMIGA_B1, HIGH);

    // The TankMouse driver treats right-button low as the wheel-code valid flag.
    _aqm_gpio_set(QM1_AMIGA_B2, LOW);

    x_move = _aqm_step_quad_axis_to(QM1_AMIGA_H, QM1_AMIGA_HQ, quad_mx_state, quad_mx_phase, target_x);
    y_move = _aqm_step_quad_axis_to(QM1_AMIGA_V, QM1_AMIGA_VQ, quad_my_state, quad_my_phase, target_y);

    busy_wait_us_32(AQM_WHEEL_HOLD_US);

    _aqm_undo_quad_axis_move(QM1_AMIGA_V, QM1_AMIGA_VQ, quad_my_state, quad_my_phase, y_move);
    _aqm_undo_quad_axis_move(QM1_AMIGA_H, QM1_AMIGA_HQ, quad_mx_state, quad_mx_phase, x_move);

    started_at = time_us_32();
    while (_aqm_gpio_active(QM1_AMIGA_B3)
        && ((uint32_t)(time_us_32() - started_at) < AQM_WHEEL_RESPONSE_TIMEOUT_US)) {
        tight_loop_contents();
    }

    _aqm_gpio_set(QM1_AMIGA_B1, button_pressed[AQM_LEFT] ? LOW : HIGH);
    _aqm_gpio_set(QM1_AMIGA_B2, button_pressed[AQM_RIGHT] ? LOW : HIGH);
}

static void _aqm_handle_tankmouse_request(
    uint8_t *quad_mx_state,
    uint8_t *quad_my_state,
    uint8_t *quad_mx_phase,
    uint8_t *quad_my_phase)
{
    uint8_t code;

    wheel_requests++;

    if (button_pressed[AQM_LEFT] || button_pressed[AQM_MIDDLE] || button_pressed[AQM_RIGHT]) {
        _aqm_wheel_clear();
        return;
    }

    if (_aqm_wheel_dequeue(&code)) {
        wheel_responses++;
        _aqm_tankmouse_respond(code, quad_mx_state, quad_my_state, quad_mx_phase, quad_my_phase);
    }
}

void amiga_quad_mouse_init()
{
    // obtain the pins we want to use
    gpio_init(QM1_AMIGA_H);
    gpio_init(QM1_AMIGA_V);
    gpio_init(QM1_AMIGA_HQ);
    gpio_init(QM1_AMIGA_VQ);
    gpio_init(QM1_AMIGA_B1);
    gpio_init(QM1_AMIGA_B2);
    gpio_init(QM1_AMIGA_B3);

    gpio_set_function(QM1_AMIGA_H, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_V, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_HQ, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_VQ, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B1, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B2, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B3, GPIO_FUNC_SIO);

    // pins are active low, so when they are at 0 they're triggering; set all high (off)
    _aqm_gpio_set(QM1_AMIGA_H, HIGH);
    _aqm_gpio_set(QM1_AMIGA_V, HIGH);
    _aqm_gpio_set(QM1_AMIGA_HQ, HIGH);
    _aqm_gpio_set(QM1_AMIGA_VQ, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B1, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B2, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B3, HIGH);

    queue_init(&motion_queue, sizeof(aqm_motion_t), AQM_MOTION_QUEUE_DEPTH);
    queue_init(&wheel_queue, sizeof(uint8_t), AQM_WHEEL_QUEUE_DEPTH);

    // start the mouse motion loop on core1
    multicore_launch_core1(amiga_quad_mouse_motion);
}

void amiga_quad_mouse_button(enum amiga_quad_mouse_buttons button, bool pressed)
{
    // ahprintf("[aqm] button %s state %s\n",
    //     (button == AQM_LEFT) ? "left" :
    //         (button == AQM_MIDDLE) ? "middle" :
    //         (button == AQM_RIGHT) ? "right" : "<unknown?!>",
    //     pressed ? "down" : "up"
    // );

    button_pressed[button] = pressed;

    switch (button) {
        case AQM_LEFT:      _aqm_gpio_set(QM1_AMIGA_B1, pressed ? LOW : HIGH); break;
        case AQM_MIDDLE:    _aqm_gpio_set(QM1_AMIGA_B3, pressed ? LOW : HIGH); break;
        case AQM_RIGHT:     _aqm_gpio_set(QM1_AMIGA_B2, pressed ? LOW : HIGH); break;
        // default:            ahprintf("[aqm] unhandled button press!\n");
    }
}

void amiga_quad_mouse_wheel(int8_t wheel)
{
    dbgcons_mouse_wheel(wheel);

    if (button_pressed[AQM_LEFT] || button_pressed[AQM_MIDDLE] || button_pressed[AQM_RIGHT])
        return;

    while (wheel > 0) {
        _aqm_wheel_enqueue(AQM_TANKMOUSE_WHEEL_UP);
        wheel--;
    }

    while (wheel < 0) {
        _aqm_wheel_enqueue(AQM_TANKMOUSE_WHEEL_DOWN);
        wheel++;
    }
}

void amiga_quad_mouse_set_motion(int16_t in_x, int16_t in_y)
{
    aqm_motion_t motion = { in_x, in_y };

    if (!queue_try_add(&motion_queue, &motion)) {
        aqm_motion_t merged;

        // When the queue is full, coalesce with a queued sample rather than
        // dropping motion outright.
        if (queue_try_remove(&motion_queue, &merged)) {
            merged.x = _aqm_add_clamped(merged.x, in_x);
            merged.y = _aqm_add_clamped(merged.y, in_y);

            if (queue_try_add(&motion_queue, &merged))
                return;
        }

        queue_try_add(&motion_queue, &motion);
    }
}

void amiga_quad_mouse_motion()
{
    // ahprintf("[aqm] hello from core1, mouse motion output loop starting\n");
    aqm_motion_t motion;
    int16_t out_x = 0, out_y = 0;
    int16_t x_residue = 0, y_residue = 0;
    uint8_t quad_mx_state = 1, quad_my_state = 1;
    uint8_t quad_mx_phase = 0, quad_my_phase = 0;
    uint8_t divider;
    bool last_mmb_state = _aqm_gpio_active(QM1_AMIGA_B3);
    uint32_t next_motion_at = time_us_32();

    /**
     * a little note about quadrature motion state.
     *
     * quadrature motion works by having a hardware-side counter for each axis and two signal
     * lines per axis. motion is signalled in an offset time division; the main axis pulse
     * changes state on time 0 and time 1, and the second signal line at time interval 0.5 and
     * 1.5, giving four possible states for each t/2. this occurs on both x and y axis.
     *
     * adcd has a crude ascii timing diagram but it explains it better:
     * https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017F.html
     */

    while (1) {
        bool mmb_state = _aqm_gpio_active(QM1_AMIGA_B3);

        if (mmb_state && !last_mmb_state)
            _aqm_handle_tankmouse_request(&quad_mx_state, &quad_my_state, &quad_mx_phase, &quad_my_phase);
        last_mmb_state = mmb_state;

        // Merge newly queued USB deltas between quadrature steps so we do not
        // lose motion across cores or wait for an entire stale batch to drain.
        divider = motion_divider ? motion_divider : 1;

        while (queue_try_remove(&motion_queue, &motion)) {
            x_residue = _aqm_add_clamped(x_residue, motion.x);
            y_residue = _aqm_add_clamped(y_residue, motion.y);
        }

        out_x = _aqm_add_clamped(out_x, x_residue / divider);
        out_y = _aqm_add_clamped(out_y, y_residue / divider);
        x_residue %= divider;
        y_residue %= divider;

        if ((out_x == 0) && (out_y == 0)) {
            tight_loop_contents();
            continue;
        }

        if ((int32_t)(time_us_32() - next_motion_at) < 0) {
            tight_loop_contents();
            continue;
        }

        if (out_x != 0) {
            // handle x-axis motion
            if (out_x < 0)
                _aqm_step_quad_axis(QM1_AMIGA_H, QM1_AMIGA_HQ, &quad_mx_state, &quad_mx_phase, -1);
            else if (out_x > 0)
                _aqm_step_quad_axis(QM1_AMIGA_H, QM1_AMIGA_HQ, &quad_mx_state, &quad_mx_phase, 1);
        }

        if (out_x < 0) out_x++;
        if (out_x > 0) out_x--;

        if (out_y != 0) {
            // handle y-axis motion
            if (out_y < 0)
                _aqm_step_quad_axis(QM1_AMIGA_V, QM1_AMIGA_VQ, &quad_my_state, &quad_my_phase, -1);
            else if (out_y > 0)
                _aqm_step_quad_axis(QM1_AMIGA_V, QM1_AMIGA_VQ, &quad_my_state, &quad_my_phase, 1);
        }

        if (out_y < 0) out_y++;
        if (out_y > 0) out_y--;

        next_motion_at = time_us_32() + AQM_STEP_INTERVAL_US;
    }
}
