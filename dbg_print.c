#include <bsp/board.h>
#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>
#include <stdarg.h>

#include <tusb.h>
#include "debug.h"

#if DEBUG

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

#endif
