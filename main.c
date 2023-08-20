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
//#include <pio_usb.h>
#include "external/pico-pio-usb/src/pio_usb.h"

#if !defined(USE_SECONDARY_USB)
#define USE_SECONDARY_USB 0
#endif

#define DEBUG_TAG "main"

#include "host.h"
#include "debug.h"

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

void led_blinking_task(void);
void core1_main(void);

int main(void) {
  // need 120MHz for USB
  //set_sys_clock_khz(120000, true);

  stdio_init_all();
  printf("Hello world");
  while (true) {
    sleep_ms(1000);
    printf("Hello world");
  }
#if false
  sleep_ms(10);

  DEBUG_INIT();

#if USE_SECONDARY_USB
  // Initialize Core 1, and put PIO-USB on it with TinyUSB
  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  board_init();
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
#endif

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
// Core 1
//
void core1_main(void) {
  sleep_ms(10);

  // Use tuh_configure() to pass pio configuration to the host stack
  // Note: tuh_configure() must be called before
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
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
  host->kbd_report( (hid_keyboard_report_t const*) report );
}

void tusb_on_mouse_report(const hid_mouse_report_t* report)
{
  host->mouse_report( (hid_mouse_report_t const*) report );
}