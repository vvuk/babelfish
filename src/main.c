/*
 * USB2Sun
 *  Connect a USB keyboard and mouse to your Sun workstation!
 *
 * Joakim L. Gilje
 * Adapted from the TinyUSB Host examples
 */

#include <pico/stdlib.h>
#include <bsp/board.h>
#include <tusb.h>
#include <pio_usb.h>

#if !defined(USE_SECONDARY_USB)
#define USE_SECONDARY_USB 0
#endif

#define DEBUG_TAG "main"

#include "host.h"
#include "debug.h"
#include "babelfish.h"

HOST_PROTOTYPES(sun);
HOST_PROTOTYPES(adb);
HOST_PROTOTYPES(apollo);

static HostDevice hosts[] = {
  HOST_ENTRY(sun),
  HOST_ENTRY(adb),
  HOST_ENTRY(apollo),
  { 0 }
};

// TODO read from flash
static int g_current_host_index = 2;

HostDevice *host = NULL;
OutputUartDevice out_uart;
KeyboardEvent static_kbd_events[NUM_STATIC_EVENTS];
MouseEvent static_mouse_events[NUM_STATIC_EVENTS];

void led_blinking_task(void);
void core1_main(void);

void babelfish_uart_config(int uidx, char ab) {
  // reset
  irq_set_enabled(UART0_IRQ, true);
  irq_set_enabled(UART1_IRQ, true);

  gpio_set_function(U0_TX_A_GPIO, GPIO_FUNC_NULL);
  gpio_set_function(U0_RX_A_GPIO, GPIO_FUNC_NULL);
  gpio_set_function(U1_TX_A_GPIO, GPIO_FUNC_NULL);
  gpio_set_function(U1_RX_A_GPIO, GPIO_FUNC_NULL);

  gpio_set_function(U0_TX_B_GPIO, GPIO_FUNC_NULL);
  gpio_set_function(U0_RX_B_GPIO, GPIO_FUNC_NULL);
  gpio_set_function(U1_TX_B_GPIO, GPIO_FUNC_NULL);
  gpio_set_function(U1_RX_B_GPIO, GPIO_FUNC_NULL);

  if (uidx == 0) {
    out_uart.uart_tx_gpio = ab == 'a' ? U0_TX_A_GPIO : U0_TX_B_GPIO;
    out_uart.uart_rx_gpio = ab == 'a' ? U0_RX_A_GPIO : U0_RX_B_GPIO;
  } else if (uidx == 1) {
    out_uart.uart_tx_gpio = ab == 'a' ? U1_TX_A_GPIO : U1_TX_B_GPIO;
    out_uart.uart_rx_gpio = ab == 'a' ? U1_RX_A_GPIO : U1_RX_B_GPIO;
  }

  gpio_set_function(out_uart.uart_tx_gpio, GPIO_FUNC_UART);
  gpio_set_function(out_uart.uart_rx_gpio, GPIO_FUNC_UART);
  gpio_set_inover(out_uart.uart_rx_gpio, GPIO_OVERRIDE_NORMAL);
  gpio_set_outover(out_uart.uart_tx_gpio, GPIO_OVERRIDE_NORMAL);

  out_uart.uart = uidx == 0 ? uart0 : uart1;
  out_uart.uart_irq = uidx == 0 ? UART0_IRQ : UART1_IRQ;
}

int main(void) {
  // need 120MHz for USB
  set_sys_clock_khz(120000, true);

  stdio_init_all();
  sleep_ms(10);

  DEBUG_INIT();

  babelfish_uart_config(0, 'a');

#if USE_SECONDARY_USB
  // Initialize Core 1, and put PIO-USB on it with TinyUSB
  multicore_reset_core1();
  board_init();

  multicore_launch_core1(core1_main);
#else
  board_init();
  tusb_init();
#endif

  host = &hosts[g_current_host_index];

  // TODO: read hostid from storage
  host->init();

  DBG("Initialized, host '%s'\n", host->name);

  while (true) {
    #if !USE_SECONDARY_USB
    tuh_task();
    #endif
    led_blinking_task();

    host->update();
  }

  return 0;
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//
// Core 1 -- secondary USB port
//
void core1_main(void) {
  sleep_ms(10);

  // Use tuh_configure() to pass pio configuration to the host stack
  // Note: tuh_configure() must be called before
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pinout = PIO_USB_PINOUT_DMDP;
  pio_cfg.pin_dp = USB_AUX_DP_GPIO;

  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

  // To run USB SOF interrupt in core1, init host stack for pio_usb (roothub
  // port1) on core1
  tuh_init(1);

  while (true) {
    tuh_task(); // tinyusb host task
  }
}

void tusb_on_kbd_report(const hid_keyboard_report_t* report)
{
  translate_boot_kbd_report(report, host);
}

void tusb_on_mouse_report(const hid_mouse_report_t* report)
{
  translate_boot_mouse_report(report, host);
}