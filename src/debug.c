#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>
#include <stdarg.h>

#include <tusb.h>
#include "babelfish.h"
#include "hid_codes.h"

#if DEBUG

static mutex_t debug_mutex;

static void debug_chars_available(void*);
static void debug_queue_fake_keypress(char ch);
static void debug_out(const char* str, int length);
static int debug_in(char* str, int length);
static void debug_in_char(char ch);
static bool debug_connected();

#define USB_DEBUG_TIMEOUT_US 50

void
debug_init()
{
    mutex_init(&debug_mutex);

    tud_init(0);

    // wait for host to connect to CDC
    absolute_time_t until = make_timeout_time_ms(200);
    do {
        if (debug_connected()) {
            sleep_ms(10);
            break;
        }
        sleep_ms(10);
    } while (!time_reached(until));
}

void
debug_task()
{
    tud_task();

    static char buf[128];
    int len = debug_in(buf, sizeof(buf));
    if (len > 0) {
        debug_in_char(buf[0]);
    }
}

bool debug_connected() {
    // below doesn't check DTR, cdc_connected does
    //return tud_ready();
    return tud_cdc_connected();
}

void tud_njamount_cb() {
    gpio_put(11, 1);
}

void tud_unmount_cb() {
    gpio_put(11, 0);
}

void
dbg(const char* tag, const char *fmt, ...)
{
    // not great for stack size, make this static?
    // would have to handle multicore though
    char buf[128];
    int len = 0;

    if (tag) {
        len = snprintf(buf, sizeof(buf), "(%s:%d) ", tag, get_core_num());
    }

    va_list args;
    va_start(args, fmt);
    len += vsnprintf(buf + len, sizeof(buf) - len, fmt, args);
    va_end(args);

    char *nl, *bb = buf;
    int remaining = len;
    while ((nl = strchr(bb, '\n')) != NULL) {
        int n = nl - bb;
        debug_out(bb, n);
        debug_out("\r\n", 2);
        remaining -= n + 1;
        bb += n + 1;
    }

    if (remaining > 0)
        debug_out(bb, remaining);
}

void
debug_out(const char* buf, int length)
{
    static uint64_t last_avail_time;
    if (!mutex_try_enter_block_until(&debug_mutex, make_timeout_time_ms(PICO_STDIO_DEADLOCK_TIMEOUT_MS))) {
        return;
    }
    if (debug_connected()) {
        for (int i = 0; i < length;) {
            int n = length - i;
            int avail = (int) tud_cdc_write_available();
            if (n > avail) n = avail;
            if (n) {
                int n2 = (int) tud_cdc_write(buf + i, (uint32_t)n);
                tud_task();
                tud_cdc_write_flush();
                i += n2;
                last_avail_time = time_us_64();
            } else {
                tud_task();
                tud_cdc_write_flush();
                if (!debug_connected() ||
                    (!tud_cdc_write_available() && time_us_64() > last_avail_time + USB_DEBUG_TIMEOUT_US)) {
                    break;
                }
            }
        }
    } else {
        // reset our timeout
        last_avail_time = 0;
    }
    mutex_exit(&debug_mutex);
}

int debug_in(char *buf, int length) {
    // these are just checks of state, so we can call them while not holding the lock.
    // they may be wrong, but only if we are in the middle of a tud_task call, in which case at worst
    // we will mistakenly think we have data available when we do not (we will check again), or
    // tud_task will complete running and we will check the right values the next time.
    int rc = PICO_ERROR_NO_DATA;
    if (!debug_connected() || !tud_cdc_available())
        return rc;

    if (!mutex_try_enter_block_until(&debug_mutex, make_timeout_time_ms(PICO_STDIO_DEADLOCK_TIMEOUT_MS)))
        return rc; // avoid deadlock

    if (debug_connected() && tud_cdc_available()) {
        int count = (int) tud_cdc_read(buf, (uint32_t) length);
        rc = count ? count : PICO_ERROR_NO_DATA;
    } else {
        // because our mutex use may starve out the background task, run tud_task here (we own the mutex)
        tud_task();
    }

    mutex_exit(&debug_mutex);

    return rc;
}

static void
debug_queue_fake_keypress(char ch)
{
#if !BABELFISH_TEST
    if (ch >= 128)
        return;

    uint8_t keycode = ascii_to_hid[ch][1];
    uint8_t modifier = ascii_to_hid[ch][0] ? HID_KEY_LEFT_SHIFT : 0;

    // ctrl-A to ctrl-Z, except for the ones that have whitespace meaning
    if (keycode == 0 && ch > 0 && ch < 27) {
        keycode = ascii_to_hid[ch + 96][1];
        modifier = HID_KEY_LEFT_CONTROL;
    }

    if (keycode == 0)
        return;

    KeyboardEvent evt = { 0 };

    evt.down = true;

    if (modifier) {
        evt.keycode = modifier;
        enqueue_kbd_event(&evt);
    }

    evt.keycode = keycode;
    enqueue_kbd_event(&evt);

    evt.down = false;
    enqueue_kbd_event(&evt);
    
    if (modifier) {
        evt.keycode = modifier;
        enqueue_kbd_event(&evt);
    }

    //dbg("Sending key '%c' (0x%02x) as %d 0x%04x\n", ch, ch, report.modifier, report.keycode[0]);
    //host->kbd_report(&report);
#endif
}

void
debug_in_char(char ch)
{
    static bool in_esc = false;
    static bool in_motion = false;

#if false
    if (ch == 0x1B) { // ESC
        in_esc = true;
        return;
    }
    
    if (in_esc) {
        if (ch == '[') {
            in_motion = true;
        } else {
            debug_queue_fake_keypress(0x1B);
            in_esc = false;
            goto process_char;
        }

        // translate arrow keys to mouse motion 
        if (in_motion) {
            hid_mouse_report_t report = { 0 };
            if (ch == 'A') {
                report.y = 1;
            } else if (ch == 'B') {
                report.y = -1;
            } else if (ch == 'C') {
                report.x = 1;
            } else if (ch == 'D') {
                report.x = -1;
            }
            //host->mouse_event(&report);
        }

        in_esc = in_motion = false;
        return;
    }
#endif

process_char:
    debug_queue_fake_keypress(ch);
}

// The below is straight direct UART debugging.

#elif DEBUG_DIRECT_UART

#define UART_DEBUG_ID uart0
#define UART_DEBUG_IRQ UART0_IRQ
#define UART_DEBUG_TX_PIN 0
#define UART_DEBUG_RX_PIN 1

static void debug_irq_handler();

void
debug_init()
{
    gpio_set_function(UART_DEBUG_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_DEBUG_RX_PIN, GPIO_FUNC_UART);
    uart_init(UART_DEBUG_ID, 115200);
    uart_set_hw_flow(UART_DEBUG_ID, false, false);
    uart_set_format(UART_DEBUG_ID, 8, 1, UART_PARITY_NONE);

    irq_set_exclusive_handler(UART_DEBUG_IRQ, debug_irq_handler);
    irq_set_enabled(UART_DEBUG_IRQ, true);
    uart_set_irq_enables(UART_DEBUG_ID, true, false);
}

void
debug_task()
{
}

void
debug_out(const char* str, int length)
{
    while (length--) {
        uart_putc_raw(UART_DEBUG_ID, *str++);
    }
}

void
debug_irq_handler()
{
    while (uart_is_readable(UART_DEBUG_ID)) {
        uint8_t ch = uart_getc(UART_DEBUG_ID);

        debug_in_char(ch);
    }
}

#endif
