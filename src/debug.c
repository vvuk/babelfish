#include <bsp/board.h>
#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>
#include <stdarg.h>

#include <tusb.h>
#include "host.h"
#include "debug.h"
#include "hid_codes.h"

#if DEBUG

static uint8_t const ascii_to_hid[128][2] = { HID_ASCII_TO_KEYCODE };

static void debug_chars_available(void*);

static void
debug_xmit_char(char ch)
{
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
}

static void
debug_process_char(char ch)
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
            debug_xmit_char(0x1B);
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
    debug_xmit_char(ch);
}

void
debug_init()
{
    stdio_init_all();
    stdio_set_chars_available_callback(debug_chars_available, NULL);
}

void
debug_chars_available(void* param)
{
    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        debug_process_char((uint8_t) ch);
    }
}

void
dbg(const char* tag, const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    puts("(");
    puts(tag);
    puts(") ");
    puts(buf);
}

// The below is straight direct UART debugging.

#elif DEBUG_DIRECT_UART

void debug_irq_handler();

void
debug_process()
{
}

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
dbg(const char* tag, const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char *bp = buf;
    uart_putc_raw(UART_DEBUG_ID, '(');
    while (*tag) {
        uart_putc_raw(UART_DEBUG_ID, *tag++);
    }
    uart_putc_raw(UART_DEBUG_ID, ')');
    uart_putc_raw(UART_DEBUG_ID, ' ');

    while (*bp) {
        if (*bp == '\n') {
            uart_putc_raw(UART_DEBUG_ID, '\r');
        }
        uart_putc_raw(UART_DEBUG_ID, *bp++);
    }
}

void
debug_irq_handler()
{
    bool in_esc = false;
    bool in_motion = false;

    while (uart_is_readable(UART_DEBUG_ID)) {
        uint8_t ch = uart_getc(UART_DEBUG_ID);
        if (ch == 0x1B) { // ESC
            in_esc = true;
            continue;
        }
        
        if (in_esc) {
            if (ch == '[') {
                in_motion = true;
                continue;
            }
            
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
                in_esc = in_motion = false;
                continue;
            }
        }

        if (ch < 128) {
            uint8_t keycode = ascii_to_hid[ch][1];
            uint8_t modifier = ascii_to_hid[ch][0] ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;

            // ctrl-A to ctrl-Z, except for the ones that have whitespace meaning
            if (keycode == 0 && ch > 0 && ch < 27) {
                keycode = ascii_to_hid[ch + 96][1];
                modifier = KEYBOARD_MODIFIER_LEFTCTRL;
            }

            if (keycode == 0)
                continue;

            hid_keyboard_report_t report = {
                .modifier = modifier,
                .keycode = { keycode }
            };
            //dbg("Sending key '%c' (0x%02x) as %d 0x%04x\n", ch, ch, report.modifier, report.keycode[0]);
            //host->kbd_report(&report);
        }
    }

    if (in_esc) {
        // if we got here without a code, move on
        uart_putc_raw(UART_DEBUG_ID, 0x1B);
    }
}

#endif
