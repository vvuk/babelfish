#include <bsp/board.h>
#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>
#include <stdarg.h>

#include <tusb.h>
#include "host.h"
#include "debug.h"

#if DEBUG

void debug_irq_handler();

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
dbg(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    char *bp = buf;
    while (*bp) {
        if (*bp == '\n') {
            uart_putc_raw(UART_DEBUG_ID, '\r');
        }
        uart_putc_raw(UART_DEBUG_ID, *bp++);
    }
    va_end(args);
}

static uint8_t const ascii_to_hid[128][2] = { HID_ASCII_TO_KEYCODE };

void
debug_irq_handler()
{
    while (uart_is_readable(UART_DEBUG_ID)) {
        uint8_t ch = uart_getc(UART_DEBUG_ID);
        if (ch < 128) {
            hid_keyboard_report_t report = {
                .modifier = ascii_to_hid[ch][0],
                .keycode = { ascii_to_hid[ch][1] }
            };
            host->kbd_report(&report);
        }
    }
}

#endif
