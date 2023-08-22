/*
 * USB2Sun
 *  Connect a USB keyboard and mouse to your Sun workstation!
 *
 * Joakim L. Gilje
 * Adapted from the TinyUSB Host examples
 */

#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <tusb.h>
#include <pio_usb.h>

#define DEBUG_TAG "main"

#include "host.h"
#include "debug.h"
#include "babelfish.h"

#define USB_ON_CORE1 1

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
KeyboardEvent kbd_event_queue[MAX_QUEUED_EVENTS];
MouseEvent mouse_event_queue[MAX_QUEUED_EVENTS];
uint8_t kbd_event_queue_count = 0;
uint8_t mouse_event_queue_count = 0;
mutex_t event_queue_mutex;

void usb_host_setup(void);
void core1_main(void);
void mainloop(void);

int main(void)
{
  // need 120MHz for USB
  set_sys_clock_khz(120000, true);

  stdio_init_all();
  sleep_ms(10);

  DEBUG_INIT();

  mutex_init(&event_queue_mutex);

  babelfish_uart_config(0, 'a');

  // Initialize Core 1, and put PIO-USB on it with TinyUSB
  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  host = &hosts[g_current_host_index];

  // TODO: read hostid from storage
  host->init();

  DBG("Initialized, host '%s'\n", host->name);

  mainloop();

  return 0;
}

void mainloop(void)
{
  KeyboardEvent kbd_events[MAX_QUEUED_EVENTS];
  MouseEvent mouse_events[MAX_QUEUED_EVENTS];
  uint kbd_event_count = 0;
  uint mouse_event_count = 0;

  while (true) {
    mutex_enter_blocking(&event_queue_mutex);
    get_queued_kbd_events(kbd_events, &kbd_event_count);
    get_queued_mouse_events(mouse_events, &mouse_event_count);
    mutex_exit(&event_queue_mutex);

    for (uint i = 0; i < kbd_event_count; i++) {
      host->kbd_event(kbd_events[i]);
    }

    for (uint i = 0; i < mouse_event_count; i++) {
      host->mouse_event(mouse_events[i]);
    }

    host->update();
  }
}

void usb_host_setup()
{
  // Use tuh_configure() to pass pio configuration to the host stack
  // Note: tuh_configure() must be called before
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pinout = PIO_USB_PINOUT_DMDP;
  pio_cfg.pin_dp = USB_AUX_DP_GPIO;

  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

  // To run USB SOF interrupt in core1, init host stack for pio_usb (roothub
  // port1) on core1
  tuh_init(1);
}

//
// Core 1 -- secondary USB port
//
void core1_main(void)
{
  sleep_ms(10);

  usb_host_setup();

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

void enqueue_kbd_event(const KeyboardEvent* event)
{
  mutex_enter_blocking(&event_queue_mutex);
  if (kbd_event_queue_count < MAX_QUEUED_EVENTS) {
    kbd_event_queue[kbd_event_queue_count++] = *event;
  }
  mutex_exit(&event_queue_mutex);
}

void enqueue_mouse_event(const MouseEvent* event)
{
  mutex_enter_blocking(&event_queue_mutex);
  if (mouse_event_queue_count < MAX_QUEUED_EVENTS) {
    mouse_event_queue[mouse_event_queue_count++] = *event;
  }
  mutex_exit(&event_queue_mutex);
}

void get_queued_kbd_events(KeyboardEvent* events, uint* count)
{
  mutex_enter_blocking(&event_queue_mutex);
  *count = kbd_event_queue_count;
  memcpy(events, kbd_event_queue, sizeof(KeyboardEvent) * kbd_event_queue_count);
  kbd_event_queue_count = 0;
  mutex_exit(&event_queue_mutex);
}

void get_queued_mouse_events(MouseEvent* events, uint* count)
{
  mutex_enter_blocking(&event_queue_mutex);
  *count = mouse_event_queue_count;
  memcpy(events, mouse_event_queue, sizeof(MouseEvent) * mouse_event_queue_count);
  mouse_event_queue_count = 0;
  mutex_exit(&event_queue_mutex);
}
