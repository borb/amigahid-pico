/**
 * ssd1306 display handling via i2c dma
 *
 * this code was very heavily inspired by https://github.com/fivdi/pico-i2c-dma; however, it was largely rewritten
 * as much of that code is considerate of multiple devices and uses freertos. this was chopped and replaced with an
 * interrupt-driven batch queue (though in truth nothing should race that hard that we ever need >1 operation in
 * flight, but is there for safety).
 *
 * @todo might be worth thinking about timeslicing the second cpu core: the mouse event loop will spend most of
 * its life sitting in delay & minimum signal interval to ensure the receiving system can handle the mouse motion
 * smoothly, so it's largely wasted. then this could be shifted to that core.
 */

#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"

#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "display/disp_ssd.h"
#include "display/ugui.h"

#define SSD_WIDTH           128
#define SSD_HEIGHT          64
#define SSD_BAUD            1000000         // 1MHz
#define SSD_ADDR            0x3c            // my board has 0x78 jumper soldered closed on the back but ¯\_(ツ)_/¯
#define I2C_MAX_TRANSFER    0x400 + 0x20    // 1KB + 32B response

/**
 * ssd1306 commands; borrowed from https://github.com/fivdi/pico-i2c-dma ssd1306 example code with thanks.
 * especially for the very comprehensive descriptions.
 */
typedef enum {
  // Fundamental commands
  SET_CONTRAST = 0x81,         // Double byte command to select 1 out of
                               // 256 contrast steps.
  SET_ENTIRE_DISP_ON = 0xa4,   // Bit0 = 0: Output follows RAM content.
                               // Bit0 = 1: Output ignores RAM content,
                               //           all pixels are turned on.
  SET_NORMAL_INVERTED = 0xa6,  // Bit0 = 0: Normal display.
                               // Bit0 = 1: Inverted display.
  SET_DISP_ON_OFF = 0xae,      // Bit0 = 0: Display off, sleep mode.
                               // Bit0 = 1: Display on, normal mode.

  // Addressing setting Commands
  SET_ADDRESSING_MODE = 0x20,  // Double byte command to set memory
                               // addressing mode.
  SET_COLUMN_ADDRESS = 0x21,   // Triple byte command to setup column start
                               // and end address.
  SET_PAGE_ADDRESS = 0x22,     // Triple byte command to setup page start and
                               // end address.

  // Hardware configuration (panel resolution and layout related) commands
  SET_DISP_START_LINE = 0x40,  // Set display RAM display start line
                               // register. Valid values are 0 to 63.
  SET_SEGMENT_REMAP = 0xa0,    // Bit 0 = 0: Map col addr 0 to SEG0.
                               // Bit 0 = 1: Map col addr 127 to SEG0.
  SET_MUX_RATIO = 0xa8,        // Double byte command to configure display
                               // height. Valid height values are 15 to 63.
  SET_COM_OUTPUT_DIR = 0xc0,   // Bit 3 = 0: Scan from 0 to N-1.
                               // Bit 3 = 1: Scan from N-1 to 0. (N=height)
  SET_DISP_OFFSET = 0xd3,      // Double byte command to configure vertical
                               // display shift. Valid values are 0 to 63.
  SET_COM_PINS_CONFIG = 0xda,  // Double byte command to set COM pins
                               // hardware configuration.

  // Timing and driving scheme setting commands
  SET_DCLK_FOSC = 0xd5,        // Double byte command to set display clock
                               // divide ratio and oscillator frequency.
  SET_PRECHARGE_PERIOD = 0xd9, // Double byte command to set pre-charge
                               // period.
  SET_VCOM_DESEL_LEVEL = 0xdb, // Double byte command to set VCOMH deselect
                               // level.

  // Charge pump command
  SET_CHARGE_PUMP = 0x8d,      // Double byte command to enable/disable
                               // charge pump.
                               // Byte2 = 0x10: Disable charge pump.
                               // Byte2 = 0x14: Enable charge pump.
} ssd1306_command_t;

// controller transfer flags: stop means "finished", abort means "something failed so i gave up"
static volatile bool stop, abort;

// display things: pixel command buffer (1 command followed by (x * y)/8)
static uint8_t display_command[1 + ((SSD_WIDTH * SSD_HEIGHT) / 8)];
// the above buffer has a write command (0x40) followed by the framebuffer; point to the start of the framebuffer
static uint8_t *framebuffer = &display_command[1];

// ugui's instance
static UG_GUI gui;

// convert from ugui's vernacular to a proper word
typedef UG_COLOR UG_COLOUR;

/**
 * Interrupt service routine for i2c device: is called on transaction abort or completion
 *
 * @return void
 */
static void disp_ssd_i2c_irqh(void)
{
    uint32_t status = i2c_get_hw(I2C_PORT)->intr_stat;

    // check for trans abort; read register causes clear to occur
    if (status & I2C_IC_INTR_STAT_R_TX_ABRT_BITS) {
        i2c_get_hw(I2C_PORT)->clr_tx_abrt;
        abort = true;
    }

    // check for trans stop (complete); read register causes clear to occur
    if (status & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
        i2c_get_hw(I2C_PORT)->clr_stop_det;
        stop = true;
    }
}

/**
 * Set a pin to open drain
 *
 * @param uint pin  The pin to set open drain
 */
static void open_drain(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, GPIO_IN);
    gpio_put(pin, 0);
}

/**
 * Check if the i2c bus is signalling a blockage
 *
 * @return bool true if blocked, false if not
 */
static bool check_blocked(void)
{
    bool sda_lev, scl_lev;

    open_drain(I2C_PIN_SDA);
    open_drain(I2C_PIN_SCL);

    sda_lev = gpio_get(I2C_PIN_SDA);
    scl_lev = gpio_get(I2C_PIN_SCL);

    return !sda_lev || !scl_lev;
}

/**
 * Forcibly unblock the i2c bus by flip/flopping the clock line in and out
 *
 * @return void
 */
static void unblock(void)
{
    open_drain(I2C_PIN_SDA);
    open_drain(I2C_PIN_SCL);

    bool sda_lev = gpio_get(I2C_PIN_SDA);
    uint32_t delay_ct,
             tries,
             drain_delay = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS) / 200;

    // try to clear the bus 9 times; if it doesn't clear, meh, just carry on; it _might_ be ok
    for (tries = 0; (tries < 9) && !sda_lev; tries++) {
        // flip the clock between level & drain and re-check the sda line; when it's high, the bus is cleared
        gpio_set_dir(I2C_PIN_SCL, GPIO_OUT);
        for (delay_ct = 0; delay_ct < drain_delay; delay_ct++)
            __asm__("nop");

        gpio_set_dir(I2C_PIN_SCL, GPIO_IN);
        for (delay_ct = 0; delay_ct < drain_delay; delay_ct++)
            __asm__("nop");

        sda_lev = gpio_get(I2C_PIN_SDA);
    }
}

/**
 * Initialise the i2c controller, and set up the interrupt service routine
 *
 * @return void
 */
void disp_i2c_init(void)
{
    // aaaand if, say, an rp2041 arrives with >2 i2c interfaces, this is where the breakage exists :D
    uint irqn = (I2C_PORT == i2c0) ? I2C0_IRQ : I2C1_IRQ;

    // disable isr whilst we're changing i2c settings
    irq_set_enabled(irqn, false);

    // since this is a fresh init, set these to false
    abort = false;
    stop = false;

    // unblock bus, if it's blocked
    if (check_blocked())
        unblock();

    // init i2c interface, setup baud rate
    i2c_init(I2C_PORT, SSD_BAUD);

    // claim pins as i2c
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);

    // set stop and abort interrupt flags
    i2c_get_hw(I2C_PORT)->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS | I2C_IC_INTR_MASK_M_TX_ABRT_BITS;

    // activate the isr for the i2c interrupt
    irq_set_exclusive_handler(irqn, disp_ssd_i2c_irqh);
    irq_set_enabled(irqn, true);
}

/**
 * Configure a receive DMA channel for an i2c device and start the DMA operation.
 *
 * @param int rx_chan           DMA channel to use for receiving data
 * @param uint8_t *read_buffer  Address of uint8_t buffer to read data into
 * @param size_t read_length    Number of bytes to expect to read (DMA op will conclude once length reached or 'stop' received)
 * @return void
 */
static void configure_rx_channel(int rx_chan, uint8_t *read_buffer, size_t read_length)
{
    dma_channel_config rx_config = dma_channel_get_default_config(rx_chan);
    channel_config_set_read_increment(&rx_config, false);
    channel_config_set_write_increment(&rx_config, true);
    channel_config_set_transfer_data_size(&rx_config, DMA_SIZE_8);
    channel_config_set_dreq(&rx_config, i2c_get_dreq(I2C_PORT, false));
    dma_channel_configure(rx_chan, &rx_config, read_buffer, &i2c_get_hw(I2C_PORT)->data_cmd, read_length, true);
}

/**
 * Configure a transmit DMA channel for an i2c device and start the DMA operation.
 * Note: The write_buffer is uint16 encoded, as a pair of <i2c instruction><device instruction>, and the constants
 * for this can be found in "hardware/i2c.h" in the pico sdk.
 *
 * @param int tx_chan               DMA channel to use for transmission
 * @param uint16_t *write_buffer    Address of uint16_t buffer containing i2c/device instruction pairs
 * @param size_t write_length       Length of write buffer (in bytes, ergo one uint16_t = 2)
 * @return void
 */
static void configure_tx_channel(int tx_chan, uint16_t *write_buffer, size_t write_length)
{
    dma_channel_config tx_config = dma_channel_get_default_config(tx_chan);
    channel_config_set_read_increment(&tx_config, true);
    channel_config_set_write_increment(&tx_config, false);
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_16);
    channel_config_set_dreq(&tx_config, i2c_get_dreq(I2C_PORT, true));
    dma_channel_configure(tx_chan, &tx_config, &i2c_get_hw(I2C_PORT)->data_cmd, write_buffer, write_length, true);
}

/**
 * Perform a read/write transaction to the i2c device
 *
 * @param uint8_t *write_buffer Location of the stream to write out
 * @param size_t write_length   Length of the stream to write
 * @param uint8_t *read_buffer  Location of the buffer to read back into
 * @param size_t read_length    Length of the stream to read
 * @return void
 */
void disp_i2c_trans(uint8_t *write_buffer, size_t write_length, uint8_t *read_buffer, size_t read_length)
{
    // commands are sent as 16-bit transactions, including the i2c action (start/stop, 0 means just "carry on sending
    // until another instruction")
    // @todo i've made this static, but potentially it doesn't need to be; the queue-driven isr should mean that the
    //       disp_i2c_trans is only triggered when the previous one has completed, so the buffer can be reset and
    //       reused. so this can probably stay where it is, but the 'static' moniker can be lost, and this can
    //       continue to be used by the active dma operation providing it's not rewritten until the isr gets the 'stop'
    //       and enqueues the next action.
    static uint16_t data_commands[I2C_MAX_TRANSFER];

    if ((write_length + read_length) > I2C_MAX_TRANSFER)
        // cowardly refuse to go over maximum transfer size
        // @todo at some point, if this happens, log, complain, dequeue and continue
        panic(
            "disp_i2c_trans() exceeded maximum allowed buffer size (read: 0x%x, write: 0x%x, max: 0x%x)\n",
            read_length,
            write_length,
            I2C_MAX_TRANSFER
        );

    const bool writing = write_length > 0,
               reading = read_length > 0;

    size_t pos;

    int tx_chan = 0,
        rx_chan = 0;

    if (writing) {
        for (pos = 0; pos != write_length; ++pos)
            data_commands[pos] = write_buffer[pos];

        data_commands[0] |= I2C_IC_DATA_CMD_RESTART_BITS;
    }

    tx_chan = dma_claim_unused_channel(true);

    if (reading) {
        for (pos = 0; pos != read_length; ++pos)
            data_commands[write_length + pos] = I2C_IC_DATA_CMD_CMD_BITS;

        data_commands[write_length] |= I2C_IC_DATA_CMD_RESTART_BITS;

        rx_chan = dma_claim_unused_channel(true);
    }

    data_commands[write_length + read_length - 1] |= I2C_IC_DATA_CMD_STOP_BITS;

    i2c_get_hw(I2C_PORT)->enable = 0;
    i2c_get_hw(I2C_PORT)->tar = SSD_ADDR;
    i2c_get_hw(I2C_PORT)->enable = 1;

    stop = false;
    abort = false;

    // assign the dma channels and start the transfer
    if (reading)
        configure_rx_channel(rx_chan, read_buffer, read_length);

    configure_tx_channel(tx_chan, data_commands, write_length + read_length);

    // @todo THIS IS THE POINT WHERE THE TRANSFER IN/OUT HAS STARTED; ideally, we stop now and return control to the caller,
    //       and let the isr pick up the continuation (abort/stop). what follows is not that, and needs moving to that isr.
    sleep_ms(100); // block for 100ms; this is not permanent

    // if an abort happened (not end of transmission), abort dma
    if (abort || !stop) {
        dma_channel_abort(tx_chan);
        if (reading)
            dma_channel_abort(rx_chan);
    }

    dma_channel_unclaim(tx_chan);
    if (reading)
        dma_channel_unclaim(rx_chan);

    // check for issues and reinitialise the i2c port if so
    if (abort || !stop)
        disp_i2c_init();
}

/**
 * Write a byte to a register on the i2c device
 *
 * @param uint8_t devregister   Register to write to
 * @param uint8_t byte          Byte to write
 * @return void
 */
static inline void write_byte(uint8_t devregister, uint8_t byte)
{
    // write a byte to a register
    uint8_t buffer[2] = {devregister, byte};
    disp_i2c_trans(buffer, 2, NULL, 0);
}

/**
 * Redraw the SSD1306 display
 *
 * @return void
 */
static void disp_ssd_update()
{
    disp_i2c_trans(display_command, sizeof(display_command), NULL, 0);
}

/**
 * UGUI callback: Translate ugui pixel draw to the ssd1306 display buffer
 *
 * @param UG_S16 x      x position to draw to
 * @param UG_S16 y      y position to draw to
 * @param UG_S16 colour Colour of pixel
 * @return void
 */
static void ugui_draw_pixel_cb(UG_S16 x, UG_S16 y, UG_COLOUR colour)
{
    if ((x < 0) ||
        (x > (SSD_WIDTH - 1)) ||
        (y < 0) ||
        (y > (SSD_HEIGHT - 1))
    ) {
        // we don't want your mucky offscreen pixels here. what do you think this is, a snes?
        return;
    }

    uint8_t *byte = &framebuffer[((y / 8) * SSD_WIDTH) + x];
    uint8_t bitmask = 1 << (y % 8);

    switch (colour) {
        case C_BLACK:
            *byte &= ~bitmask; // clear pixel
            break;
        case C_WHITE:
            *byte |= bitmask; // set pixel
            break;
        default:
            *byte ^= bitmask; // invert pixel for anything else (should we just panic tho?)
    }
}

/**
 * Setup the i2c and ssd1306 display ready for use. Without calling this, behaviour is undefined.
 *
 * @return void
 */
void disp_ssd_init(void)
{
    const uint8_t init_sequence[] = {
        SET_DISP_ON_OFF | 0x00,         // Display off.
        SET_DCLK_FOSC, 0x80,            // Set clock divide ratio and oscillator
                                        //   frequency.
        SET_MUX_RATIO, 0x3f,            // Set display height.
        SET_DISP_OFFSET, 0x00,          // Set vertical display shift to 0.
        SET_DISP_START_LINE,            // Set display RAM display start line
                                        //   register to 0.
        SET_CHARGE_PUMP, 0x14,          // Enable charge pump.
        SET_SEGMENT_REMAP | 0x01,       // Map col addr 127 to SEG0.
        SET_COM_OUTPUT_DIR | 0x08,      // Scan from N-1 to 0. (N=height)
        SET_COM_PINS_CONFIG, 0x12,      // Set COM pins hardware configuration to
                                        //   0x12.
        SET_CONTRAST, 0xcf,             // Set contrast to 0xcf
        SET_PRECHARGE_PERIOD, 0xf1,     // Set pre-charge to 0xf1
        SET_VCOM_DESEL_LEVEL, 0x40,     // Set VCOMH deselect to 0x40
        SET_ENTIRE_DISP_ON,             // Output follows RAM content.
        SET_NORMAL_INVERTED | 0x00,     // Normal display.
        SET_ADDRESSING_MODE, 0x00,      // Set addressing mode to horizontal mode.
        SET_COLUMN_ADDRESS, 0x00, 0x7f, // Set column start and end address.
        SET_PAGE_ADDRESS, 0x00, 0x07,   // Set page start and end address.
        SET_DISP_ON_OFF | 0x01,         // Display on.
    };

    // start the i2c device control
    disp_i2c_init();

    // commands sent to the ssd1306 are written to a single location on register 0x80, and
    // as such cannot be sent as a block. even command parameters must be sent to the same
    // register, so it all has to go, one byte at a time, waiting for the isr to signal stop
    // after each byte.
    for (uint pos = 0; pos < sizeof(init_sequence); pos++)
        write_byte(0x80, init_sequence[pos]);

    // set the command at the beginning of the framebuffer to write out
    display_command[0] = SET_DISP_START_LINE;

    // setup ugui, blank the display
    UG_Init(&gui, ugui_draw_pixel_cb, SSD_WIDTH, SSD_HEIGHT);
    UG_SetBackcolor(C_BLACK);
    UG_SetForecolor(C_WHITE);
    UG_FillScreen(C_BLACK);
    UG_FontSelect(&FONT_5X12);
}

/**
 * Write a message to the display
 *
 * @param uint8_t x     x offset (in characters)
 * @param uint8_t y     y offset (in characters)
 * @return void
 */
void disp_write(uint8_t x, uint8_t y, char *message)
{
    UG_S16 px = x * 5,
           py = y * 12;

    UG_PutString(px, py, message);
    disp_ssd_update();
}
