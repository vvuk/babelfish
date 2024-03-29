#include <tusb.h>

#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>

#define DEBUG_TAG "sun"
#include "babelfish.h"

#include "host_sun_keycodes.h"

#define UART_KEYBOARD_NUM 0
#define UART_KEYBOARD uart0
#define UART_KEYBOARD_IRQ UART0_IRQ

static void on_keyboard_rx();

void sun_keyboard_uart_init() {
	// Apollo expects 5V serial, not RS-232 voltages.
	channel_config(0, ChannelModeLevelShifter | ChannelModeUART | ChannelModeInvert);

  uart_init(UART_KEYBOARD, 1200);
  uart_set_hw_flow(UART_KEYBOARD, false, false);
  uart_set_format(UART_KEYBOARD, 8, 1, UART_PARITY_NONE);
  irq_set_exclusive_handler(UART_KEYBOARD_IRQ, on_keyboard_rx);
  irq_set_enabled(UART_KEYBOARD_IRQ, true);
  uart_set_irq_enables(UART_KEYBOARD, true, false);
}

// RX interrupt handler
void on_keyboard_rx() {
    while (uart_is_readable(UART_KEYBOARD)) {
        // printf("System command: ");
        uint8_t ch = uart_getc(UART_KEYBOARD);

        switch (ch) {
          case 0x01: // reset
            // printf("Reset\n");
            uart_putc_raw(UART_KEYBOARD, 0xff);
            uart_putc_raw(UART_KEYBOARD, 0x04);
            uart_putc_raw(UART_KEYBOARD, 0x7f);
            break;
          case 0x02: // bell on
            // printf("Bell on\n");
            break;
          case 0x03: // bell off
            // printf("Bell off\n");
            break;
          case 0x0a: // click on
            // printf("Click on\n");
            break;
          case 0x0b: // click off
            // printf("Click off\n");
            break;
          case 0x0e: // led command
            // printf("Led\n");
            {
              uint8_t led = uart_getc(UART_KEYBOARD);
            }
            break;
          case 0x0f: // layout command
            // printf("Layout\n");
            uart_putc_raw(UART_KEYBOARD, 0xfe);
            uart_putc_raw(UART_KEYBOARD, 0x00);
            break;
          default:
            // printf("Unknown system command: 0x%02x\n", ch);
            break;
        };
    }
}

void sun_kbd_event(const KeyboardEvent event) {
  // if the gui/sun-extra-keys modifier is pressed
  static bool gui = false;
  static uint32_t keys_down = 0;

  if (event.page != 0)
    return;

  if (EVENT_IS_HOST_MOD(event)) {
    gui = event.down;
    return;
  }

  if (event.down) {
    keys_down++;
  } else {
    keys_down--;
  }

#define SEND_SUN_KEY(suncode, down) uart_putc_raw(UART_KEYBOARD, down ? (suncode) : ((suncode) | 0x80))

  if (gui) {
    switch (event.keycode) {
      case HID_KEY_F1: SEND_SUN_KEY(SUN_KEY_STOP, event.down); break;
      case HID_KEY_F2: SEND_SUN_KEY(SUN_KEY_AGAIN, event.down); break;
      case HID_KEY_1: SEND_SUN_KEY(SUN_KEY_PROPS, event.down); break;
      case HID_KEY_2: SEND_SUN_KEY(SUN_KEY_UNDO, event.down); break;
      case HID_KEY_Q: SEND_SUN_KEY(SUN_KEY_FRONT, event.down); break;
      case HID_KEY_W: SEND_SUN_KEY(SUN_KEY_COPY, event.down); break;
      case HID_KEY_A: SEND_SUN_KEY(SUN_KEY_OPEN, event.down); break;
      case HID_KEY_S: SEND_SUN_KEY(SUN_KEY_PASTE, event.down); break;
      case HID_KEY_Z: SEND_SUN_KEY(SUN_KEY_FIND, event.down); break;
      case HID_KEY_X: SEND_SUN_KEY(SUN_KEY_CUT, event.down); break;
    }

    return;
  }

  if (usb2sun[event.keycode] != 0) {
    SEND_SUN_KEY(usb2sun[event.keycode], event.down);
  }

  if (keys_down == 0) {
    uart_putc_raw(UART_KEYBOARD, 0x7f);
  }
}