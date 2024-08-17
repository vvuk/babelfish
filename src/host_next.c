#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>
#include <tusb.h>

#include <hardware/pio.h>
#include <hardware/clocks.h>
#include "next.pio.h"

#include "hid_codes.h"

#define DEBUG_VERBOSE 0
#define DEBUG_TAG "next"

#include "babelfish.h"

#define SOUNDBOX_OUT_GPIO TX_B_GPIO
#define SOUNDBOX_IN_GPIO TX_A_GPIO
#define SOUNDBOX_CLK_IN_GPIO RX_A_GPIO

// These names are confusing because they are from the persepctive
// of the host, not the soundbox
// Monitor-IN -- "in from monitor"
#define MIN_GPIO SOUNDBOX_OUT_GPIO
// Monitor-OUT -- "out to monitor"
#define MOUT_GPIO SOUNDBOX_IN_GPIO
#define MCLK_GPIO SOUNDBOX_CLK_IN_GPIO

#define SM_RX 0
#define SM_TX 1

// Assumption: pio0 is taken by tinyusb
#define NEXT_PIO pio1

//
// NeXT protocol values/codes
//

// Bits for address value.
#define KM_NO_RESPONSE 0x40
#define KM_USER_POLL 0x20
#define KM_INVALID 0x10
#define KM_MASTER 0x10
#define KM_ADDR_MASK 0x0f
#define KM_ADDR 0x0e

#define KM_KBD_ADDR   0x00
#define KM_MOUSE_ADDR 0x01

#define ASIC_REV_OLD 0x00
#define ASIC_REV_NEW 0x01
#define ASIX_REV_DIGITAL 0x02

#define KBD_KEY_VALID   0x8000
#define KBD_MOD_MASK    0x7F00
#define KBD_KEY_UP      0x0080
#define KBD_KEY_MASK    0x007F

#define KMS_MAGIC_MASK      0x7000FFFF /* must be normal internal event from master device */
#define KMS_MAGIC_RESET     0x1000A825 /* asterisk and left alt and left command key */
#define KMS_MAGIC_NMI_L     0x10008826 /* backquote and left command key */
#define KMS_MAGIC_NMI_R     0x10009026 /* backquote and right command key */
#define KMS_MAGIC_NMI_LR    0x10009826 /* backquote and both command keys */

// Bits for data value.

#define KD_KEYMASK	0x007f
#define KD_DIRECTION	0x0080 /* pressed or released; if set, released */

#define KD_CNTL		0x01
#define KD_LSHIFT	0x02
#define KD_RSHIFT	0x04
#define KD_LCOMM	0x08
#define KD_RCOMM	0x10
#define KD_LALT		0x20
#define KD_RALT		0x40
#define KD_VALID	0x80 /* only set for scancode keys ? */
#define KD_MODS		0x4f

#define MAX_NUM_WORDS 8
static uint32_t s_recv_words[MAX_NUM_WORDS];
static uint32_t s_recv_next_index = 0;

static void next_rx_irq(void);
static void send_command_with_data(uint8_t command, uint32_t data);
static void send_command(uint8_t command);
static void send_key(uint8_t modifiers, uint8_t keycode, bool down);
static void check_mouse_xmit(void);

// Some helpers

static void pio_sm_set_pin_as_output(PIO pio, uint sm, uint gpio)
{
    pio_gpio_init(pio, gpio);
    pio_sm_set_consecutive_pindirs(pio, sm, gpio, 1, true);
}

static void pio_sm_set_pin_as_input(PIO pio, uint sm, uint gpio)
{
    pio_sm_set_consecutive_pindirs(pio, sm, gpio, 1, false);
}

static void decode_words(const uint32_t* words, uint32_t* cmdp, uint32_t* datap)
{
    uint32_t a = words[0];
    uint32_t b = words[1];

    if (a == 0x7ff && b == 0xffffffff) {
        // reset
        *cmdp = 0xff;
        return;
    }

    // a has start bit + 8 bits command + 2 bits of data (if any, or stop bits)
    if ((a & 0b010000000000) == 0) {
        DBG("bad framing: no start bit 0x%08x\n", a);
        *cmdp = 0;
        return;
    }

    *cmdp = (uint8_t) ((a & 0b01111111100) >> 2);
    if ((*cmdp & 0b11000000) == 0b11000000) {
        // has data
        uint32_t data = (a & 0b11) << 30;
        data |= b >> 2;
        *datap = data;

        if ((b & 0b11) != 0) {
            DBG("bad framing: bad stop bits 0x%08x\n", b);
        }
    } else if ((a & 0b11) != 0) {
        DBG("bad framing: bad stop bits (cmd) 0x%08x\n", a);
    }
}

//
// Initialization
//

void next_init() {
    DBG("Configuring NeXT\n");

    channel_config(0, ChannelModeGPIO | ChannelModeNoInvert | ChannelModeLevelShifter); // RX & CLK.  We're going to use TX as RX.
    channel_config(1, ChannelModeGPIO | ChannelModeNoInvert | ChannelModeDirect); // TX -- note no shifter, we're using TTL into CMOS

    gpio_set_function(SOUNDBOX_IN_GPIO, GPIO_FUNC_NULL);
    gpio_set_drive_strength(SOUNDBOX_OUT_GPIO, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_slew_rate(SOUNDBOX_OUT_GPIO, GPIO_SLEW_RATE_FAST);

    DBG("Configuring PIO\n");

    //float clk_div = clock_get_hz(clk_sys) / 5000000.0f;
    float clk_div = 2.0f;

    DBG("Clock divider: %2.3f\n", clk_div);

    // bypass input synchronizer on CLK and IN pins
    hw_set_bits(&NEXT_PIO->input_sync_bypass, 1u << SOUNDBOX_CLK_IN_GPIO);
    hw_set_bits(&NEXT_PIO->input_sync_bypass, 1u << SOUNDBOX_IN_GPIO);

    pio_sm_claim(NEXT_PIO, SM_RX);
    pio_sm_claim(NEXT_PIO, SM_TX);

    DBG("RX: %d TX: %d\n", SM_RX, SM_TX);

    if (SOUNDBOX_CLK_IN_GPIO != (SOUNDBOX_IN_GPIO+1)) {
        printf("BAD CLK GPIO! Must be +1 from IN GPIO\n");
        assert(false);
    }

    pio_sm_set_pin_as_input(NEXT_PIO, SM_RX, SOUNDBOX_IN_GPIO);
    pio_sm_set_pin_as_input(NEXT_PIO, SM_RX, SOUNDBOX_CLK_IN_GPIO);

    pio_sm_set_pin_as_output(NEXT_PIO, SM_TX, SOUNDBOX_OUT_GPIO);
    pio_sm_set_pin_as_input(NEXT_PIO, SM_TX, SOUNDBOX_CLK_IN_GPIO);

    irq_set_exclusive_handler(PIO1_IRQ_0, next_rx_irq);
    irq_set_enabled(PIO1_IRQ_0, true);

    pio_set_irq0_source_enabled(NEXT_PIO, pis_interrupt0, true);
    pio_interrupt_clear(NEXT_PIO, 0);

    pio_sm_config cfg;
    
    uint offset_rx = pio_add_program(NEXT_PIO, &next_rx_program);
    cfg = next_rx_program_get_default_config(offset_rx);
    sm_config_set_clkdiv(&cfg, clk_div);
    sm_config_set_in_pins(&cfg, SOUNDBOX_IN_GPIO);
    sm_config_set_in_shift(&cfg, false /* false: shift left */, false /* enabled */, 0);

    //sm_config_set_sideset(&cfg, 1, /* optional */ true, /* pindirs */ false);
    //sm_config_set_sideset_pins(&cfg, LED_AUX_GPIO);

    pio_sm_init(NEXT_PIO, SM_RX, offset_rx, &cfg);

    uint offset_tx = pio_add_program(NEXT_PIO, &next_tx_program);
    cfg = next_tx_program_get_default_config(offset_tx);
    sm_config_set_clkdiv(&cfg, clk_div);
    sm_config_set_in_pins(&cfg, SOUNDBOX_CLK_IN_GPIO);
    sm_config_set_out_pins(&cfg, SOUNDBOX_OUT_GPIO, 1);
    sm_config_set_set_pins(&cfg, SOUNDBOX_OUT_GPIO, 1);
    sm_config_set_out_shift(&cfg, false /* false: shift left; MSB first */, false /* enabled */, 0);

    pio_sm_init(NEXT_PIO, SM_TX, offset_tx, &cfg);

    DBG("Enabling PIO\n");

    pio_sm_set_enabled(NEXT_PIO, SM_RX, true);
    pio_sm_set_enabled(NEXT_PIO, SM_TX, true);
}

void next_rx_irq(void)
{
    // There will always be two words ready to read. There shouldn't
    // ever be _more_ than two for now given how we synchronize.
    while (!pio_sm_is_rx_fifo_empty(NEXT_PIO, SM_RX)) {
        s_recv_words[s_recv_next_index++] = pio_sm_get(NEXT_PIO, SM_RX);
        s_recv_words[s_recv_next_index++] = pio_sm_get(NEXT_PIO, SM_RX);
        s_recv_next_index = s_recv_next_index % MAX_NUM_WORDS;
    }
    pio_interrupt_clear(NEXT_PIO, 0);
}

static bool next_ready = false;
static bool saw_reset = false;

static void process_incoming()
{
    uint32_t words[8];
    uint32_t cmd;
    uint32_t data;

    if (!s_recv_next_index)
        return;

    // copy the buffer and let pio keep receiving
    int cnt = s_recv_next_index;
    memcpy(words, s_recv_words, cnt * 4);
    s_recv_next_index = 0;
    pio_sm_put(NEXT_PIO, SM_RX, 0);

    decode_words(words, &cmd, &data);

    bool is_reset = cmd == 0xff;
    if (is_reset) {
        if (!saw_reset) {
            DBG("saw first reset\n");
        }
        saw_reset = true;
        return;
    }

    // if it's not a reset, but we never saw one, ignore; garbage
    if (!saw_reset)
        return;

    switch (cmd) {
        case 0xc5:
            if (data == 0xef000000) {
                // set address; ignore becase we're a "new" ASIC
                // and keyboard and mouse have fixed locations
                DBG("set address (igoring)\n");
                send_command_with_data(0xc6, 0x70000000);
            } else if ((data >> 24) == 0x00) {
                // some LED stuff. not exactly but close enough.
                DBG("set LEDs (todo)\n");
                send_command_with_data(0xc6, 0x70000000);
            }
            break;
        case 0xc6:
            //DBG("set poll mask and interval (0x%08x)\n", data);
            send_command(0x00); // ?? sometimes this is a 0x00, sometimes a 0x01
            next_ready = true;
            break;
        default:
            DBG("unknown cmd: 0x%02x data: 0x%08x\n", cmd, data);
            break;
    }
}

void next_update() {
    // process incoming commands
    process_incoming();
    check_mouse_xmit();
}

void send_command_with_data(uint8_t command, uint32_t data)
{
  // write MSB first; MSB values are written first and
  // OSR is shifted left

  // need to include the start bit and the two stop bits
  // Sccccccc cddddddd dddddddd ddddddddd
  uint32_t d0 = (1u<<31) | (command << 23) | (data >> 9);
  // dddddddd dQQ...... ........ ........
  uint32_t d1 = data << 23;

  pio_sm_put(pio1, SM_TX, 8+32+3);
  pio_sm_put(pio1, SM_TX, d0);
  pio_sm_put(pio1, SM_TX, d1);
}

void send_command(uint8_t command)
{
  uint32_t d0 = (1u<<31) | (command << 23) | 0;
  pio_sm_put(pio1, SM_TX, 8+3);
  pio_sm_put(pio1, SM_TX, d0);
}

void send_key(uint8_t modifiers, uint8_t keycode, bool down)
{
    uint8_t addr = 0x00 | KM_MASTER;
    uint32_t data = 0;

    data |= (addr << 24);
    data |= (modifiers << 8) | keycode | KBD_KEY_VALID | (down ? 0 : KBD_KEY_UP);

    DBG("0xc6 0x%08x\n", data);
    send_command_with_data(0xc6, data);
}

// defined at end of file
// [256] = hid code, result is NeXT scancode
static uint8_t s_next_scan_table[256];

void next_kbd_event(const KeyboardEvent event) {
    static uint32_t modifiers = 0;
    #define MODKEY(mkey, set) do { if (set) { modifiers |= mkey; } else { modifiers &= ~mkey; } } while (0)

    //if (!next_ready)
    //    return;

    uint16_t keycode = event.keycode;

    if (keycode == HID_KEY_F4) keycode = HID_KEY_LEFT_GUI;
    else if (keycode == HID_KEY_F5) keycode = HID_KEY_RIGHT_GUI;

    // update modifier key state
    switch (keycode) {
        case HID_KEY_LEFT_CONTROL:
        case HID_KEY_RIGHT_CONTROL: MODKEY(KD_CNTL, event.down); break;
        case HID_KEY_LEFT_SHIFT:  MODKEY(KD_LSHIFT, event.down); break;
        case HID_KEY_RIGHT_SHIFT: MODKEY(KD_RSHIFT, event.down); break;
        case HID_KEY_LEFT_ALT:    MODKEY(KD_LALT, event.down); break;
        case HID_KEY_RIGHT_ALT:   MODKEY(KD_RALT, event.down); break;
        case HID_KEY_LEFT_GUI:    MODKEY(KD_LCOMM, event.down); break;
        case HID_KEY_RIGHT_GUI:   MODKEY(KD_RCOMM, event.down); break;
    }

    if (keycode <= 0xff) {
        uint8_t scancode = s_next_scan_table[keycode];
        if (scancode != 0)
            send_key(modifiers, scancode, event.down);
    }
}

// report mouse at most 1000/200 times per second
#define MOUSE_RATE_MS 100
#define SPEED_DIV 2

static int mouse_cdx = 0;
static int mouse_cdy = 0;
static int mouse_cbtn = 0;
static int mouse_lbtn = 0;
static uint32_t mouse_last_report = 0;

void check_mouse_xmit() {
	if (mouse_cdx == 0 && mouse_cdy == 0 && mouse_cbtn == mouse_lbtn)
		return;

	uint32_t now_ms = to_ms_since_boot(get_absolute_time());
	if (now_ms - mouse_last_report >= MOUSE_RATE_MS || mouse_cbtn != mouse_lbtn) {

		// slow down
		int cdx = mouse_cdx / SPEED_DIV;
		int cdy = mouse_cdy / SPEED_DIV;

		// clamp
		int8_t tdx = cdx > 127 ? 127 : cdx < -127 ? -127 : cdx;
		int8_t tdy = cdy > 127 ? 127 : cdy < -127 ? -127 : cdy;

		DBG_VV("mouse xmit: tdx %d tdy %d\n", tdx, tdy);

        // .. send ..

		mouse_cdx = 0;
		mouse_cdy = 0;
		mouse_lbtn = mouse_cbtn;
		mouse_last_report = now_ms;
	}
}

void next_mouse_event(const MouseEvent event)
{
	mouse_cdx += event.dx;
	mouse_cdy += event.dy;
	mouse_cbtn = event.buttons;
}

static uint8_t s_next_scan_table[256] = {
    // Steve Jobs didn't believe in function keys, but we'll use these
    // for volume/brightness (same map as macbook)
    [HID_KEY_F1]                    = 0x01, // brightness down
    [HID_KEY_F2]                    = 0x19, // brightness up
    [HID_KEY_F3]                    = 0,
    [HID_KEY_F4]                    = 0,
    [HID_KEY_F5]                    = 0,
    [HID_KEY_F6]                    = 0x58, // power
    [HID_KEY_F7]                    = 0,
    [HID_KEY_F8]                    = 0,
    [HID_KEY_F9]                    = 0,
    [HID_KEY_F10]                   = 0,
    [HID_KEY_F11]                   = 0x02, // vol down
    [HID_KEY_F12]                   = 0x1a, // vol up,

    [HID_KEY_ESCAPE]                = 0x49,
    [HID_KEY_1]                     = 0x4a,
    [HID_KEY_2]                     = 0x4b,
    [HID_KEY_3]                     = 0x4c,
    [HID_KEY_4]                     = 0x4d,
    [HID_KEY_5]                     = 0x50,
    [HID_KEY_6]                     = 0x4f,
    [HID_KEY_7]                     = 0x4e,
    [HID_KEY_8]                     = 0x1e,
    [HID_KEY_9]                     = 0x1f,
    [HID_KEY_0]                     = 0x20,
    [HID_KEY_MINUS]                 = 0x1d,
    [HID_KEY_EQUAL]                 = 0x1c,
    [HID_KEY_BACKSPACE]             = 0x1b,

    /* C1 .. C14 */
    [HID_KEY_TAB]                   = 0x41,
    [HID_KEY_Q]                     = 0x42,
    [HID_KEY_W]                     = 0x43,
    [HID_KEY_E]                     = 0x44,
    [HID_KEY_R]                     = 0x45,
    [HID_KEY_T]                     = 0x48,
    [HID_KEY_Y]                     = 0x47,
    [HID_KEY_U]                     = 0x46,
    [HID_KEY_I]                     = 0x06,
    [HID_KEY_O]                     = 0x07,
    [HID_KEY_P]                     = 0x08,
    [HID_KEY_BRACKET_LEFT]          = 0x05,
    [HID_KEY_BRACKET_RIGHT]         = 0x04,
    [HID_KEY_BACKSLASH]             = 0x03,

    [HID_KEY_LEFT_CONTROL]          = 0x57,
    [HID_KEY_A]                     = 0x39,
    [HID_KEY_S]                     = 0x3a,
    [HID_KEY_D]                     = 0x3b,
    [HID_KEY_F]                     = 0x3c,
    [HID_KEY_G]                     = 0x3d,
    [HID_KEY_H]                     = 0x40,
    [HID_KEY_J]                     = 0x3f,
    [HID_KEY_K]                     = 0x3e,
    [HID_KEY_L]                     = 0x2d,
    [HID_KEY_SEMICOLON]             = 0x2c,
    [HID_KEY_APOSTROPHE]            = 0x2b,
    [HID_KEY_ENTER]                 = 0x2a,

    [HID_KEY_LEFT_SHIFT]            = 0x56,
    [HID_KEY_Z]                     = 0x31,
    [HID_KEY_X]                     = 0x32,
    [HID_KEY_C]                     = 0x33,
    [HID_KEY_V]                     = 0x34,
    [HID_KEY_B]                     = 0x35,
    [HID_KEY_N]                     = 0x37,
    [HID_KEY_M]                     = 0x36,
    [HID_KEY_COMMA]                 = 0x2e,
    [HID_KEY_PERIOD]                = 0x2f,
    [HID_KEY_SLASH]                 = 0x30,
    [HID_KEY_RIGHT_SHIFT]           = 0x55,

    [HID_KEY_LEFT_ALT]              = 0x52,
    [HID_KEY_LEFT_GUI]              = 0x54,

    [HID_KEY_SPACE]                 = 0x38,

    [HID_KEY_RIGHT_GUI]             = 0x53,
    [HID_KEY_RIGHT_ALT]             = 0x51,

    [HID_KEY_HOME]                  = 0,
    [HID_KEY_PAGE_UP]               = 0,
    [HID_KEY_PAGE_DOWN]             = 0,
    [HID_KEY_END]                   = 0,
    [HID_KEY_DELETE]                = 0,
    [HID_KEY_CAPS_LOCK]             = 0,

    [HID_KEY_ARROW_LEFT]            = 0x09,
    [HID_KEY_ARROW_UP]              = 0x16,
    [HID_KEY_ARROW_RIGHT]           = 0x10,
    [HID_KEY_ARROW_DOWN]            = 0x0f,

    /* Keypad */
    [HID_KEY_GRAVE]                 = 0x26,
    [HID_KEY_KEYPAD_EQUAL]          = 0x27,
    [HID_KEY_KEYPAD_SLASH]          = 0x28,
    [HID_KEY_KEYPAD_ASTERISK]       = 0x25,

    [HID_KEY_KEYPAD_7_HOME]         = 0x21,
    [HID_KEY_KEYPAD_8_UP_ARROW]     = 0x22,
    [HID_KEY_KEYPAD_9_PAGEUP]       = 0x23,
    [HID_KEY_KEYPAD_MINUS]          = 0x24,

    [HID_KEY_KEYPAD_4_LEFT_ARROW]   = 0x12,
    [HID_KEY_KEYPAD_5]              = 0x18,
    [HID_KEY_KEYPAD_6_RIGHT_ARROW]  = 0x13,
    [HID_KEY_KEYPAD_PLUS]           = 0x15,

    [HID_KEY_KEYPAD_1_END]          = 0x11,
    [HID_KEY_KEYPAD_2_DOWN_ARROW]   = 0x17,
    [HID_KEY_KEYPAD_3_PAGEDN]       = 0x14,

    [HID_KEY_KEYPAD_0_INSERT]       = 0x0b,
    [HID_KEY_KEYPAD_DECIMAL]        = 0x0c,
    [HID_KEY_KEYPAD_ENTER]          = 0x0d,
};
