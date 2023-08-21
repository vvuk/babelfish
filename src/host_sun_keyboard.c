#include <tusb.h>

#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>

#include "host_sun_keycodes.h"
#include "host.h"

#define UART_KBD_ID uart1
#define UART_KBD_IRQ UART1_IRQ
#define UART_KBD_TX_PIN 4
#define UART_KBD_RX_PIN 5

static void on_keyboard_rx();

void sun_keyboard_uart_init() {
  uart_init(UART_KBD_ID, 1200);
  gpio_set_function(UART_KBD_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_KBD_RX_PIN, GPIO_FUNC_UART);
  uart_set_hw_flow(UART_KBD_ID, false, false);
  uart_set_format(UART_KBD_ID, 8, 1, UART_PARITY_NONE);
  irq_set_exclusive_handler(UART_KBD_IRQ, on_keyboard_rx);
  irq_set_enabled(UART_KBD_IRQ, true);
  uart_set_irq_enables(UART_KBD_ID, true, false);
  gpio_set_inover(UART_KBD_RX_PIN, GPIO_OVERRIDE_INVERT);
  gpio_set_outover(UART_KBD_TX_PIN, GPIO_OVERRIDE_INVERT);
}

// RX interrupt handler
void on_keyboard_rx() {
    while (uart_is_readable(UART_KBD_ID)) {
        // printf("System command: ");
        uint8_t ch = uart_getc(UART_KBD_ID);

        switch (ch) {
          case 0x01: // reset
            // printf("Reset\n");
            uart_putc_raw(UART_KBD_ID, 0xff);
            uart_putc_raw(UART_KBD_ID, 0x04);
            uart_putc_raw(UART_KBD_ID, 0x7f);
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
              uint8_t led = uart_getc(UART_KBD_ID);
            }
            break;
          case 0x0f: // layout command
            // printf("Layout\n");
            uart_putc_raw(UART_KBD_ID, 0xfe);
            uart_putc_raw(UART_KBD_ID, 0x00);
            break;
          default:
            // printf("Unknown system command: 0x%02x\n", ch);
            break;
        };
    }
}

void sun_kbd_event(KeyboardEvent* events, uint16_t count) {
  // if the gui/sun-extra-keys modifier is pressed
  static bool gui = false;
  static uint32_t keys_down = 0;

  for (uint16_t i = 0; i < count; ++i) {
    KeyboardEvent event = events[i];

    if (event.page != 0)
      continue;

    if (EVENT_IS_HOST_MOD(event)) {
      gui = event.down;
      continue;
    }

    if (event.down) {
      keys_down++;
    } else {
      keys_down--;
    }

#define SEND_SUN_KEY(suncode, down) uart_putc_raw(UART_KBD_ID, down ? (suncode) : ((suncode) | 0x80))

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

      continue;
    }

    if (usb2sun[event.keycode] != 0) {
      SEND_SUN_KEY(usb2sun[event.keycode], event.down);
    }

    if (keys_down == 0) {
      uart_putc_raw(UART_KBD_ID, 0x7f);
    }
  }
}