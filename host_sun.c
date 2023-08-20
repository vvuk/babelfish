#include <bsp/board.h>
#include <tusb.h>

extern void sun_keyboard_uart_init();
extern void sun_mouse_uart_init();
extern void sun_mouse_tx();

void sun_init() {
    sun_keyboard_uart_init();
    sun_mouse_uart_init();
}

void sun_update() {
    sun_mouse_tx();
}