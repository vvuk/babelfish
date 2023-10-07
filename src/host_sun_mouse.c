#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <tusb.h>

#define DEBUG_TAG "sun"
#include "babelfish.h"

static bool serial_data_in_tail = false;
static bool updated = false;
static int32_t delta_x = 0;
static int32_t delta_y = 0;
#define NO_BUTTONS 0x7
static char btns = NO_BUTTONS;
static uint32_t interval = 40;

#define UART_MOUSE_NUM 1
#define UART_MOUSE uart1

void sun_mouse_uart_init() {
	DBG("Sun mouse emulation: port B (tx only).\n");
	DBG("Move shifter switch to 5V.\n");

  channel_config(UART_MOUSE_NUM, ChannelModeLevelShifter | ChannelModeUART | ChannelModeInvert);

  uart_init(UART_MOUSE, 1200);
  uart_set_hw_flow(UART_MOUSE, false, false);
  uart_set_format(UART_MOUSE, 8, 1, UART_PARITY_NONE);
}

static inline int32_t clamp(int32_t value, int32_t min, int32_t max) {
  if      (value < min) return min;
  else if (value > max) return max;
  return value;
}

static uint32_t push_head_packet() {
  uart_putc_raw(UART_MOUSE, btns | 0x80);
  uart_putc_raw(UART_MOUSE, delta_x);
  uart_putc_raw(UART_MOUSE, delta_y);
  btns = NO_BUTTONS;
  delta_x = 0;
  delta_y = 0;
  serial_data_in_tail = true;
  return 25;
}

static uint32_t push_tail_packet() {
  uart_putc_raw(UART_MOUSE, delta_x);
  uart_putc_raw(UART_MOUSE, delta_y);
  delta_x = 0;
  delta_y = 0;
  serial_data_in_tail = false;
  return 15;
}

void sun_mouse_tx() {
  static uint32_t start_ms = 0;
  if ((to_ms_since_boot(get_absolute_time()) - start_ms) < interval) {
    return;
  }
  start_ms += interval;

  if (updated) {
    if (serial_data_in_tail) {
      interval = push_tail_packet();
      updated = (btns != NO_BUTTONS);
    } else {
      interval = push_head_packet();
    }
  }
}

void sun_mouse_event(const MouseEvent event) {
  btns = ((event.buttons & MOUSE_BUTTON_LEFT)   ? 0 : 4)
      | ((event.buttons & MOUSE_BUTTON_MIDDLE) ? 0 : 2)
      | ((event.buttons & MOUSE_BUTTON_RIGHT)  ? 0 : 1)
  ;
  delta_x += event.dx;
  delta_y += -event.dy;
  delta_x = clamp(delta_x, -127, 127);
  delta_y = clamp(delta_y, -127, 127);
  updated = true;
}
