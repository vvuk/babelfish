#define DEBUG_VERBOSE 2
#define DEBUG_TAG "apollo"

#include "babelfish.h"

#define UART_A_NUM 0
#define UART_A uart0
#define UART_B_NUM 1
#define UART_B uart1

void test_3v3_init() {
    for (int u = 0; u < 2; ++u)
    {
        channel_config(u, ChannelModeDirect | ChannelModeUART | ChannelModeInvert);
        uart_init(u == 0 ? UART_A : UART_B, 1200);
        uart_set_hw_flow(u == 0 ? UART_A : UART_B, false, false);
        uart_set_format(u == 0 ? UART_A : UART_B, 8, 1, UART_PARITY_NONE);
    }
}

#define XMIT_INTERVAL_MS 200
static uint32_t last_xmit = 0;
static bool put_b = false;

void test_3v3_update() {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now > last_xmit+XMIT_INTERVAL_MS) {
        uart_putc_raw(UART_A, 'A');
        last_xmit = now;
        put_b = false;
    } else if (!put_b && now > last_xmit+(XMIT_INTERVAL_MS/2)) {
        uart_putc_raw(UART_B, 'b');
        put_b = true;
        // no last_xmit update, only A updates
    }
}

void test_3v3_kbd_event(const KeyboardEvent event) {
}

void test_3v3_mouse_event(const MouseEvent event) {
}
