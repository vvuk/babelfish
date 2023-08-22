#include <bsp/board.h>
#include <tusb.h>

#include <pico/stdlib.h>
#include <pico/time.h>

#include "host.h"

#define DEBUG_TAG "apollo"
#include "debug.h"

/*
 * NOTES:
 *
 * - In the future, maybe simplify by moving all this to core 1?
 *   busy_wait_us_32(usec)
 *   busy_wait_ 
 */

/* pin 9 */
#define ADB_GPIO 6

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

// threshold after which we assume it's a 1 and not a 0.  
#define DATA_IS_1_LOW_THRESHOLD 40

#define CMD_RESET 0
#define CMD_FLUSH 1
#define CMD_LISTEN 2
#define CMD_TALK 3

typedef enum {
    Unknown,              //  initial state
    Idle,                 // idle state, bus is high
    Reset,                //   3 ms or however long, bus goes low before returning high
    Attention,            // 800 us, low by host
    Sync,                 //  70 us, high by host
    CommandDone,          //  we just finished reading the command byte
    ListenDataDone,       //  we just finished reading the 16-bit value
    Tlt,                  // 140-260 us, high
    Srq,                  // 300 us, low by device (note: during stop bit)
    ListenDataLo,         //  35-65 us, low, data started, we'll figure out what it is based on length
    ListenData0Hi,        //  35 us, high
    ListenData1Lo,        //  35 us, low
    ListenData1Hi,        //  65 us, high
    ListenStartBitLo,
    ListenStartBitHi,
    ListenStopBitLo,      // a 0 bit
    ListenStopBitHi,      // a 
    TalkData0Lo,
    TalkData0Hi,
    TalkData1Lo,
    TalkData1Hi,
    TalkDataDone
} AdbState;

static uint16_t adb_mouse_regs[4] = { 0 };
static uint16_t adb_kbd_regs[4] = { 0 };

#define DEVICE_REGISTER(id, addr, srq, exc)  ((id) | ((addr) << 7) | ((srq) << 12) | ((exc) << 13))

#define INTERRUPTS_ON()  do { gpio_set_irq_enabled(ADB_GPIO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true); } while (0)
#define INTERRUPTS_OFF() do { gpio_set_irq_enabled(ADB_GPIO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false); } while (0)

static uint64_t last_any_transition_us = 0;
static uint64_t last_rise_us = 0;
static uint64_t last_fall_us = 0;

static AdbState in_state = Unknown;
static bool is_talking = false;
// when reading/sending data bits, the expected number of bits (1 or 8 or 16)
static int data_expected_bits = 0;
// when we're done reading/sending data bits, what state to transition to?
static AdbState data_next_state = Unknown;
// if we're sending data bits, what value are we sending?
// if we're reading, what value did we just read? (in data_next_state)
static uint16_t data_value = 0;

// when the command bits finish, we still have a Stop bit and Tlt to get through.
// but after that what we actually do depends on the command; this state specifies that.
static AdbState command_next_state = Unknown;

static void adb_isr();

void adb_init() {
    adb_kbd_regs[3]   = DEVICE_REGISTER(0, 2, 1, 1); // handler 0, default kbd id (2), enable srq, disable exc (1)
    adb_mouse_regs[3] = DEVICE_REGISTER(0, 3, 1, 1);

    gpio_set_function(ADB_GPIO, GPIO_FUNC_SIO);
    gpio_set_irq_enabled_with_callback(ADB_GPIO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &adb_isr);
    // gpio_set_ir
}

void adb_update() {
    uint64_t cur_time = time_us_64();
    if (cur_time - last_any_transition_us > 1000) {
        // we haven't seen a transition in a while; reset state
        in_state = Idle;
    }
}

void adb_kbd_event(const KeyboardEvent event) {
}

void adb_mouse_event(const MouseEvent event) {
}

uint8_t cmd_addr = 0;
uint8_t cmd_cmd = 0;
uint8_t cmd_reg = 0;

void handle_command(uint8_t command_byte) {
    cmd_addr = (command_byte >> 4) & 0xf;
    cmd_cmd = (command_byte >> 2) & 3;
    cmd_reg = command_byte & 3;

    DBG("command: addr: %d, cmd: %d, reg: %d\n", cmd_addr, cmd_cmd, cmd_reg);
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

void handle_listen_data(uint16_t data) {
    DBG("listen data: %d\n", data);
}

// we're going to be generous and give ourselves 30% tolerance; we'll also
// expect the compiler to turn these into integer constants

void expect_fall_after(uint64_t cur_time, bool is_rise, uint32_t time) {
    if (gpio_get(ADB_GPIO))
        DBG("expected low, state: %d\n", in_state);
    if (!is_rise)
        DBG("expected fall irq, state: %d\n", in_state);
    uint32_t min_time = time * 0.7;
    uint32_t max_time = time * 1.3;
    if (cur_time - last_rise_us < min_time || cur_time - last_rise_us > max_time)
        DBG("expected fall after %d us, got %d us, state: %d\n", time, cur_time - last_rise_us, in_state);
}

void expect_rise_after(uint64_t cur_time, bool is_rise, uint32_t time) {
    if (!gpio_get(ADB_GPIO))
        DBG("expected high, state: %d\n", in_state);
    if (is_rise)
        DBG("expected rise irq, state: %d\n", in_state);
    uint32_t min_time = time * 0.7;
    uint32_t max_time = time * 1.3;
    if (cur_time - last_fall_us < min_time || cur_time - last_fall_us > max_time)
        DBG("expected rise after %d us, got %d us, state: %d\n", time, cur_time - last_fall_us, in_state);
}

void expect_fall_after_nomax(uint64_t cur_time, bool is_rise, uint32_t time) {
    if (gpio_get(ADB_GPIO))
        DBG("expected low, state: %d\n", in_state);
    if (!is_rise)
        DBG("expected fall irq, state: %d\n", in_state);
    uint32_t min_time = time * 0.7;
    if (cur_time - last_rise_us < min_time)
        DBG("expected fall after %d us, got %d us, state: %d\n", time, cur_time - last_rise_us, in_state);
}

void expect_rise_after_nomax(uint64_t cur_time, bool is_rise, uint32_t time) {
    if (!gpio_get(ADB_GPIO))
        DBG("expected high, state: %d\n", in_state);
    if (is_rise)
        DBG("expected rise irq, state: %d\n", in_state);
    uint32_t min_time = time * 0.7;
    if (cur_time - last_fall_us < min_time)
        DBG("expected rise after %d us, got %d us, state: %d\n", time, cur_time - last_fall_us, in_state);
}


void adb_state_machine(uint64_t cur_time, bool is_rise, bool is_fall) {
    switch (in_state) {
    case Unknown:
        // we're waiting for the bus to go low.  it may have gone high when the computer
        // was turned on, but we're waiting for the explicit low.
        if (is_fall)
            in_state = Reset;
        break;
    case Reset:
        // note: these expects are the opposite of what the line of the state should be,
        // because we're handling an interrupt and checking to see if we need to get
        // out of that state
        expect_rise_after_nomax(cur_time, is_rise, RESET_TIME_US);
        in_state = Idle;
        break;
    case Idle:
        if (gpio_get(ADB_GPIO))
            DBG("expected low, state: %d\n");
        if (is_fall) {
            in_state = Attention;
        }
        break;
    case Attention:
        expect_rise_after(cur_time, is_rise, ATTENTION_TIME_US);
        in_state = Sync;
        break;
    case Sync:
        expect_fall_after(cur_time, is_rise, SYNC_TIME_US);
        data_value = 0;
        data_next_state = CommandDone;
        in_state = ListenDataLo;
        break;

    case CommandDone:
        handle_command(data_value & 0xff);
        in_state = Tlt;
        break;
    case ListenDataDone:
        handle_listen_data(data_value);
        in_state = Idle;
        break;

    case Tlt:
        expect_rise_after(cur_time, is_rise, TLT_TIME_US);
        in_state = ListenStartBitLo;
        break;

    case ListenStartBitLo:
        expect_rise_after(cur_time, is_rise, DATA_1_L_TIME_US);
        in_state = ListenStartBitHi;
        break;
    case ListenStartBitHi:
        expect_fall_after(cur_time, is_rise, DATA_1_H_TIME_US);
        in_state = ListenDataLo;
        break;
    case ListenDataLo:
        // we don't know yet whether it's a 0 or a 1
        if (!gpio_get(ADB_GPIO) || !is_rise) {
            DBG("expected high (%d) + rise irq (%d), state: %d\n", gpio_get(ADB_GPIO), is_rise, in_state);
            in_state = Idle;
            return;
        }

        if (cur_time - last_fall_us >= DATA_IS_1_LOW_THRESHOLD) {
            in_state = ListenData1Hi;
        } else {
            in_state = ListenData0Hi;
        }
        break;
    case ListenData1Hi:
        expect_fall_after(cur_time, is_rise, DATA_1_H_TIME_US);
        data_value = data_value | (1 << (data_expected_bits-1));
        data_expected_bits--;
        if (data_expected_bits == 0) {
            in_state = ListenStopBitLo;
        } else {
            in_state = ListenDataLo;
        }
        break;
    case ListenData0Hi:
        expect_fall_after(cur_time, is_rise, DATA_0_H_TIME_US);
        //data_value = data_value | (0 << (data_expected_bits-1)); // no-op
        data_expected_bits--;
        if (data_expected_bits == 0) {
            in_state = ListenStopBitLo;
        } else {
            in_state = ListenDataLo;
        }
        break;
    case ListenStopBitLo:
        expect_rise_after(cur_time, is_rise, DATA_0_L_TIME_US);
        in_state = ListenStopBitHi;
        break;
    case ListenStopBitHi:
        // nomax, because this is where Srq would get shoved in
        expect_fall_after_nomax(cur_time, is_rise, DATA_0_H_TIME_US);
        in_state = data_next_state;
        // call the state machine again to complete the transition
        adb_state_machine(cur_time, false, false);
        break;
    }
}

void adb_isr(uint gpio, uint32_t events) {
    uint64_t cur_time = time_us_64();
    bool is_rise = events & GPIO_IRQ_EDGE_RISE;
    bool is_fall = events & GPIO_IRQ_EDGE_FALL;

    adb_state_machine(cur_time, is_rise, is_fall);

    last_any_transition_us = cur_time;
    if (events & GPIO_IRQ_EDGE_RISE) {
        last_rise_us = cur_time;
    } else {
        last_fall_us = cur_time;
    }

    // note: gpio_acknowledge_irq is called automatically
}
