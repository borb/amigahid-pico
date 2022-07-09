/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga keyboard serial interface.
 */

#include "config.h"
#include "keyboard_serial_io.h"
#include "keyboard.h"
#include "keyboard.pio.h" // generated at compile time
#include "util/output.h"

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

enum _sync_state { IDLE, SYNC };
// don't optimise variables hit by the timer isr (timer callback?)
volatile enum _sync_state sync_state = IDLE;
volatile bool clock_timer_fired = false;

// caps lock will be read by the hid loop
bool caps_lock = false;

enum _keyboard_pin_state { LOW, HIGH };

// @todo this is copy-pasta from quad_mouse; move to util/io.c
static inline void _keyboard_gpio_set(uint gpio, enum _keyboard_pin_state state)
{
    if (state == LOW) {
        gpio_put(gpio, 0);
        gpio_set_dir(gpio, GPIO_OUT);
        return;
    }

    // assume it's high otherwise
    gpio_set_dir(gpio, GPIO_IN);
}

// _.-._.-._ @todo i've not been doing sync correctly for sooooooo long; fix/remove? -._.-._.-
int64_t sync_timer_cb(alarm_id_t id, void *user_data)
{
    clock_timer_fired = true;
    _keyboard_gpio_set(KBD_AMIGA_DAT, LOW);
    sync_state = SYNC;
}

void amiga_init()
{
    // setup digital mode, direction and active high/low on /clk, /dat and /rst.
    gpio_init(KBD_AMIGA_DAT);
    gpio_init(KBD_AMIGA_CLK);
    gpio_init(KBD_AMIGA_RST);

    // all pins are active low, meaning if /rst is current at 0, the amiga is held in reset.
    // rectify this by putting all pins in open drain. this should bring the amiga to boot.
    _keyboard_gpio_set(KBD_AMIGA_DAT, HIGH);
    _keyboard_gpio_set(KBD_AMIGA_CLK, HIGH);
    _keyboard_gpio_set(KBD_AMIGA_RST, HIGH);

    // now the pins are setup, setup the timer callback to maintain keyboard comms in sync.
    // @todo add_alarm_in_ms() here

    // wait a full second then send initpower, pause 200ms and then termpower. unlike the amiga kbd 6502, we're
    // not doing anything during this time, so it's just so the computer is happy in the knowledge that we are
    // here.
    sleep_ms(1000);
    amiga_send(AMIGA_INITPOWER, false);
    sleep_ms(200);
    amiga_send(AMIGA_TERMPOWER, false);

    // fin.
}

bool amiga_caps_lock()
{
    return caps_lock;
}

void amiga_send(uint8_t keycode, bool up)
{
    uint8_t bit_position, bit_mask = 0x80, sendcode;
    static bool ctrl = false, lamiga = false, ramiga = false, in_reset = false;

    // we don't care about caps lock coming up; ignore it
    if ((keycode == AMIGA_CAPSLOCK) && up)
        return;

    // check for caps lock going down and toggle; rewrite the 'up' parameter
    if (keycode == AMIGA_CAPSLOCK) {
        // amiga caps lock is odd; when it's on, it sends down code but no up, and vice versa when it comes off
        up = caps_lock;
        caps_lock = !caps_lock;

        // ahprintf("[akb] caps lock %s\n", caps_lock ? "ON" : "OFF");
    }

    if (keycode == AMIGA_CTRL)
        ctrl = !up;
    if (keycode == AMIGA_LAMIGA)
        lamiga = !up;
    if (keycode == AMIGA_RAMIGA)
        ramiga = !up;

    if ((ctrl && lamiga && ramiga) && !in_reset) {
        in_reset = true;
        amiga_assert_reset();
    }

    if (in_reset && !(ctrl && lamiga && ramiga)) {
        in_reset = false;
        amiga_release_reset();
    }

    // copy input code, roll left, move msb to lsb
    sendcode = keycode | (up == true ? 0x80 : 0x00);
    sendcode <<= 1;
    if (up || (keycode & 0x80))
        sendcode |= 1;

    for (bit_position = 0; bit_position < 8; bit_position++) {
        if (sendcode & bit_mask)
            _keyboard_gpio_set(KBD_AMIGA_DAT, LOW);
        else
            _keyboard_gpio_set(KBD_AMIGA_DAT, HIGH);

        // hold /dat for 20us before pulsing /clk, then wait 50us before next bit
        sleep_us(20);
        _keyboard_gpio_set(KBD_AMIGA_CLK, LOW);
        sleep_us(20);
        _keyboard_gpio_set(KBD_AMIGA_CLK, HIGH);
        sleep_us(50); // @todo should be 20?

        // shift the bit pattern for next iteration
        bit_mask >>= 1;
    }

    // set /dat to input for 5ms to signal end of key
    _keyboard_gpio_set(KBD_AMIGA_DAT, HIGH);
    sleep_ms(5);

    // @todo we _should_ be checking that the amiga has acked the code by watching /dat
    // for a lwo pulse. according to adcd2.1, while the computer cannot detect
    // out-of-sync, the keyboard can by looking for the pulse, sending $f9 then the
    // repeated code.
}

void amiga_assert_reset()
{
    // ahprintf("[akb] *** RESET BEING ASSERTED ***\n");
    _keyboard_gpio_set(KBD_AMIGA_RST, LOW);
}

void amiga_release_reset()
{
    // ahprintf("[akb] *** RESET BEING RELEASED ***\n");
    _keyboard_gpio_set(KBD_AMIGA_RST, HIGH);
}

void amiga_service()
{
    if ((sync_state == SYNC) && clock_timer_fired) {
        // @todo THIS IS WRONG
        _keyboard_gpio_set(KBD_AMIGA_RST, HIGH);
        sync_state = IDLE;
        clock_timer_fired = false;
    }
}
