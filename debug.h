#ifndef DEBUG_H_
#define DEBUG_H_

#if DEBUG

#define UART_DEBUG_ID uart0
#define UART_DEBUG_IRQ UART0_IRQ
#define UART_DEBUG_TX_PIN 0
#define UART_DEBUG_RX_PIN 1

void debug_init();
#define DEBUG_INIT() debug_init()

void dbg(const char* tag, const char *fmt, ...);

#ifndef DEBUG_TAG
#define DEBUG_TAG "??"
#endif

#define DBG(...) dbg(DEBUG_TAG, __VA_ARGS__)

#else

#define DEBUG_INIT() do { } while (0)
#define DBG(...) do { } while (0)

#endif

#endif