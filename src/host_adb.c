#if !defined(TESTBENCH)
#include <pico/stdlib.h>
#include <pico/time.h>
#include <tusb.h>

#define DEBUG_TAG "adb"
#include "babelfish.h"

#define CHK(cond, ...) if (!(cond)) { DBG(__VA_ARGS__); }
#else
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t time_us_64();
int gpio_get(int);
#define DBG printf
#define GPIO_IRQ_EDGE_RISE (1<<1)
#define GPIO_IRQ_EDGE_FALL (1<<2)
#define CHK(cond, ...) if (!(cond)) { printf(__VA_ARGS__); }
#endif

#define TIME_MIN(x) ((uint32_t)((x) * 0.7))
#define TIME_MAX(x) ((uint32_t)((x) * 1.3))

/*
 * NOTES:
 *
 * - ADB is an open collector bus.  The data pin is held high at +5V when idle.
 * - The host supplies minimum 500mA on the 5V pin.
 * - When looking at the pin socket, the pins are:
 *     4   3      1 - Data          3 - +5V
 *    2     1     2 - Power switch  4 - GND
 *      ===           (ground to turn on?)
 */

// timing tolerances are quite tight on the host (+/- 3% on most) but very forgiving
// on device (+/- 30%)
#define RESET_TIME_US 3000
#define ATTENTION_TIME_US 800
#define SYNC_TIME_US 70
#define STOP_TIME_US 100
#define SRQ_TIME_US 300
#define TLT_TIME_US 200
#define DATA_0_L_TIME_US 65
#define DATA_0_H_TIME_US 35
#define DATA_1_L_TIME_US 35
#define DATA_1_H_TIME_US 65

#define CMD_RESET 0
#define CMD_FLUSH 1
#define CMD_LISTEN 2
#define CMD_TALK 3

const char* CMD_NAMES[] = {
    "Reset",
    "Flush",
    "Listen",
    "Talk"
};

typedef enum {
    Unknown,              //  initial state
    Idle,                 // idle state, bus is high
    Reset,                //   3 ms or however long, bus goes low before returning high
    AttentionOrResetOrDataStart,            // Attn is 800 us, or reset 3000 us, low by host, or a device's start data bit (lo)
    Sync,                 //  70 us, high by host
    CommandDone,          //  we just finished reading the command byte
    ListenDataDone,       //  we just finished reading the 16-bit value
    Tlt,                  // 140-260 us, high, comes after Stop bit
    Srq,                  // 300 us, low by device (note: during stop bit)
    ListenDataLo,         //  35-65 us, low, data started, we'll figure out what it is based on length
    ListenData0Hi,        //  35 us, high
    ListenData1Lo,        //  35 us, low
    ListenData1Hi,        //  65 us, high
    ListenStartBitLo,
    ListenStartBitHi,
    ListenStopBit,        // a 0 bit
    TalkData0Lo,
    TalkData0Hi,
    TalkData1Lo,
    TalkData1Hi,
    TalkDataDone,
    IdleOrTlt,
} AdbState;

const char* STATE_NAMES[] = {
    "Unknown",
    "Idle",
    "Reset",
    "AttentionOrResetOrDataStart",
    "Sync",
    "CommandDone",
    "ListenDataDone",
    "Tlt",
    "Srq",
    "ListenDataLo",
    "ListenData0Hi",
    "ListenData1Lo",
    "ListenData1Hi",
    "ListenStartBitLo",
    "ListenStartBitHi",
    "ListenStopBit",
    "TalkData0Lo",
    "TalkData0Hi",
    "TalkData1Lo",
    "TalkData1Hi",
    "TalkDataDone",
    "IdleOrTlt",
};

static uint16_t s_adb_mouse_regs[4] = { 0 };
static uint16_t s_adb_kbd_regs[4] = { 0 };

#define DEVICE_REGISTER(h, addr, srq, exc)  ((h) | ((addr) << 8) | ((srq) << 13) | ((exc) << 14))

#define INTERRUPTS_ON()  do { gpio_set_irq_enabled(ADB_GPIO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true); } while (0)
#define INTERRUPTS_OFF() do { gpio_set_irq_enabled(ADB_GPIO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false); } while (0)

static uint64_t last_transition_us = 0;
static uint64_t since_last_us = 0;
static bool last_was_rise = false;

static AdbState in_state = Unknown;
static bool is_talking = false;
// when reading/sending data bits, the expected number of bits (1 or 8 or 16)
static int data_expected_bits = 0;
// when we're done reading/sending data bits, what state to transition to?
static AdbState data_next_state = Unknown;
// if we're sending data bits, what value are we sending?
// if we're reading, what value did we just read? (in data_next_state)
static uint16_t data_value = 0;

#if !defined(TESTBENCH)
static void adb_isr(unsigned int, long unsigned int);
#endif

static int ADB_GPIO = 0;

void adb_init() {
    s_adb_kbd_regs[3]   = DEVICE_REGISTER(0, 2, 1, 1); // handler 0, default kbd id (2), enable srq, disable exc (1)
    s_adb_mouse_regs[3] = DEVICE_REGISTER(0, 3, 1, 1);

#if !defined(TESTBENCH)
    channel_config(0, ChannelModeLevelShifter | ChannelModeGPIO | ChannelModeInvert);

    ADB_GPIO = channels[0].rx_gpio;

    // TODO -- the ADB bus is open-drain. So, we can only drive it low and must leave it
    // floating to go high (or to not drive it).
    // The way to do this on rp2040 is to configure it as an input when we want to leave the
    // bus high (or untouched) and configure as an output (with out = 0) when we want to drive it
    // low.
    //gpio_set_dir(ADB_GPIO, GPIO_INOUT);
    gpio_set_irq_enabled_with_callback(ADB_GPIO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &adb_isr);
#endif
}

void adb_update() {
    uint64_t cur_time = time_us_64();
    if (cur_time - last_transition_us > 1000) {
        // we haven't seen a transition in a while; reset state
        in_state = Idle;
    }
}

#if !TESTBENCH
void adb_kbd_event(const KeyboardEvent event) {
}

void adb_mouse_event(const MouseEvent event) {
}
#endif

uint8_t cmd_addr = 0;
uint8_t cmd_cmd = 0;
uint8_t cmd_reg = 0;

void handle_command(uint8_t command_byte) {
    cmd_addr = (command_byte >> 4) & 0xf;
    cmd_cmd = (command_byte >> 2) & 3;
    cmd_reg = command_byte & 3;

    DBG("==> %s($%x, r%d)\n", CMD_NAMES[cmd_cmd], cmd_addr, cmd_reg);
    if (cmd_cmd == CMD_RESET) {
    } else if (cmd_cmd == CMD_FLUSH) {
    } else if (cmd_cmd == CMD_LISTEN) {
        // we'll automatically transition to Tlt after this, after which a data read cycle will start
        data_expected_bits = 16;
        data_next_state = ListenDataDone;
    } else if (cmd_cmd == CMD_TALK) {
        // TODO
    }
}

void handle_data(uint16_t data) {
    bool is_command = cmd_cmd == CMD_LISTEN;
    DBG("====> %s data: 0x%04x (probably %s $%x)\n", is_command ? "command" : "reply", data, is_command ? "to" : "from", cmd_addr);
    if (cmd_cmd == CMD_LISTEN && cmd_reg == 3) {
        uint8_t addr = (data >> 8) & 0xf;
        uint8_t handler = data & 0xff;
        uint8_t srq_en = (data >> 13) & 1;
        uint8_t exc = (data >> 14) & 1;
        //DBG("device config register: foo: %x handler %d, addr %x, srq %d, exc %d\n", (data >> 8) & 0xf, handler, addr, srq_en, exc);
        if (addr != cmd_addr) {
            DBG("device address change: $%x => $%x\n", cmd_addr, addr);
        }
    }
}

// we're going to be generous and give ourselves 30% tolerance; we'll also
// expect the compiler to turn these into integer constants

#define CHK_GPIO_LOW() CHK(!gpio_get(ADB_GPIO), "Expected gpio LOW, state: %s\n", STATE_NAMES[in_state])
#define CHK_GPIO_HIGH() CHK(gpio_get(ADB_GPIO), "Expected gpio HIGH, state: %s\n", STATE_NAMES[in_state])
#define CHK_RISE() CHK(is_rise, "Expected rise, state: %s\n", STATE_NAMES[in_state])
#define CHK_FALL() CHK(!is_rise, "Expected FALL, state: %s\n", STATE_NAMES[in_state])

void expect_is_fall_after(bool is_rise, uint32_t time) {
    CHK_GPIO_LOW();
    CHK_FALL();
    if (since_last_us < TIME_MIN(time) || since_last_us > TIME_MAX(time))
        DBG("[%llu] expected fall after ~%d us, got %llu us, state: %d\n", time_us_64(), time, since_last_us, in_state);
}

void expect_is_rise_after(bool is_rise, uint32_t time) {
    CHK_GPIO_HIGH();
    CHK_RISE();
    if (since_last_us < TIME_MIN(time) || since_last_us > TIME_MAX(time))
        DBG("[%llu] expected rise after ~%d us, got %llu us, state: %d\n", time_us_64(), time, since_last_us, in_state);
}

void expect_is_fall_after_min(bool is_rise, uint32_t time) {
    CHK_GPIO_LOW();
    CHK_FALL();
    if (since_last_us < TIME_MIN(time))
        DBG("[%llu] expected fall after >%d us, got %llu us, state: %d\n", time_us_64(), time, since_last_us, in_state);
}

void expect_is_rise_after_min(bool is_rise, uint32_t time) {
    CHK_GPIO_HIGH();
    CHK_RISE();
    if (since_last_us < TIME_MIN(time))
        DBG("[%llu] expected rise after >%d us, got %llu us, state: %d\n", time_us_64(), time, since_last_us, in_state);
}

void adb_state_machine(uint64_t cur_time, bool is_rise) {
    AdbState last_state = in_state;
    switch (in_state) {
    case Unknown:
        // we're waiting for the bus to go low.  it may have gone high when the computer
        // was turned on, but we're waiting for the explicit low.
        if (!is_rise)
            in_state = Reset;
        break;
    case Reset:
        // note: these expects are the opposite of what the line of the state should be,
        // because we're handling an interrupt and checking to see if we need to get
        // out of that state
        if (since_last_us >= TIME_MIN(RESET_TIME_US)) {
            in_state = Idle;
        } else {
            in_state = Unknown;
        }
        break;
    case Idle:
        CHK(!is_rise, "Expected fall coming out of Idle/IdleOrTlt\n");
        in_state = AttentionOrResetOrDataStart;
        break;
    case IdleOrTlt:
        // We were just in a high period after a command, which will be a minimum of Tlt time (I think?).
        // The next fall period is going to determine whether this is a data start bit or if we're
        // back to Attn. But minimum time is Tlt. (?) Which is different from regular Idle.
        expect_is_fall_after_min(is_rise, TLT_TIME_US);
        in_state = AttentionOrResetOrDataStart;
        break;
    case AttentionOrResetOrDataStart:
        CHK(is_rise, "Expected rise coming out of Attention/Reset/Data Start\n");

        // this might also become a reset, if it goes on for too long
        if (since_last_us >= TIME_MIN(RESET_TIME_US)) { // Reset
            in_state = Idle;
        } else if (since_last_us >= TIME_MIN(ATTENTION_TIME_US)) { // Sync
            in_state = ListenStartBitHi;
            data_expected_bits = 8;
            data_next_state = CommandDone;
        } else {
            expect_is_rise_after(is_rise, DATA_1_L_TIME_US);
            in_state = ListenStartBitHi;
            data_expected_bits = 16;
            data_next_state = ListenDataDone;
        }
        break;

    case ListenStartBitHi:
        expect_is_fall_after(is_rise, DATA_1_H_TIME_US);
        in_state = ListenDataLo;
        break;

    case ListenDataLo:
        if (!gpio_get(ADB_GPIO) || !is_rise) {
            DBG("expected high (%d) + rise irq (%d), state: %d\n", gpio_get(ADB_GPIO), is_rise, in_state);
            in_state = Idle;
            return;
        }

        // Low1 is less than Low0. If we go above
        if (since_last_us >= TIME_MAX(DATA_1_L_TIME_US)) {
            in_state = ListenData0Hi;
        } else {
            in_state = ListenData1Hi;
        }
        break;
    case ListenData0Hi:
    case ListenData1Hi:
        expect_is_fall_after(is_rise, in_state == ListenData0Hi ? DATA_0_H_TIME_US : DATA_1_H_TIME_US);
        data_value <<= 1;
        if (in_state == ListenData1Hi)
            data_value |= 1;
        data_expected_bits--;
        //DBG("data_value: 0x%04x, bits left: %d\n", data_value, data_expected_bits);
        CHK(data_expected_bits >= 0, "DATA BITS UNDERFLOW");
        if (data_expected_bits == 0) {
            in_state = ListenStopBit;
        } else {
            in_state = ListenDataLo;
        }
        break;

    case ListenStopBit:
        // The stop bit is 65ms low, but any device on the bus may hold it for
        // 300us to trigger Srq. So we check how long the period was to know
        // if we saw Srq or not. We don't do anything to process Srq -- it's
        // something for the host to handle.
        if (since_last_us > SRQ_TIME_US) {
            DBG("saw SRQ");
        }

        // After the stop bit, we have a rise transition. The high period is
        // either Tlt or Idle, and will transition into either back into Attn or into a start bit.
        expect_is_rise_after_min(is_rise, DATA_0_L_TIME_US);

        if (data_next_state == CommandDone) {
            handle_command(data_value & 0xff);
            in_state = IdleOrTlt;
        } else if (data_next_state == ListenDataDone) {
            handle_data(data_value);
            in_state = Idle;
        } else {
            DBG("unexpected data_next_state: %s\n", STATE_NAMES[data_next_state]);
        }

        data_next_state = Unknown;
        break;
    }

    if (last_state != in_state)
    {
        //DBG("[%12llu][%s] state: %s -> %s\n", time_us_64(), is_rise ? "rise" : "FALL", STATE_NAMES[last_state], STATE_NAMES[in_state]);
    }
}

void adb_isr(unsigned int gpio, long unsigned int events) {
    uint64_t cur_time = time_us_64();
    bool is_rise = events & GPIO_IRQ_EDGE_RISE;
    bool is_fall = events & GPIO_IRQ_EDGE_FALL;

    if (is_rise && is_fall)
    {
        DBG("######################\n");
        DBG("Missed events, got both rise and fall!\n");
        DBG("######################\n");
    }

    since_last_us = cur_time - last_transition_us;

    //printf("%s%s state: %d\n", is_rise ? "rise" : "", is_fall ? "fall" : "", gpio_get(0));
    adb_state_machine(cur_time, is_rise);

    last_transition_us = cur_time;
    last_was_rise = is_rise;

    // note: gpio_acknowledge_irq is called automatically
}
