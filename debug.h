#ifndef DEBUG_H_
#define DEBUG_H_

#if DEBUG

#define UART_DEBUG_ID uart0
#define UART_DEBUG_IRQ UART0_IRQ
#define UART_DEBUG_TX_PIN 0
#define UART_DEBUG_RX_PIN 1

void debug_init();
#define DEBUG_INIT() debug_init()

void dbg(const char *fmt, ...);

#else

#define DEBUG_INIT() do { } while (0)
#define dbg(...) do { } while (0)

#endif

#endif