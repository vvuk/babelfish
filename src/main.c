/*
 * Babelfish
 * Copyright (C) 2023 Vladimir Vukicevic
 *
 * Multihost USB keyboard/mouse adapter
 * 
 * Originally based on USB2Sun by Joakim L. Gilje
 * Originally based on TinyUSB Host examples
 */

#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <tusb.h>
#include <pio_usb.h>

#define DEBUG_VERBOSE 0
#define DEBUG_TAG "main"
#include "debug.h"

#include "host.h"
#include "babelfish.h"

#include <pico/stdio_usb.h>

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

  stdio_usb_init();
  stdio_init_all();
  sleep_ms(100);

  DEBUG_INIT();
  DBG("==== B A B E L F I S H ====\n");

  mutex_init(&event_queue_mutex);

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
    get_queued_kbd_events(kbd_events, &kbd_event_count);
    get_queued_mouse_events(mouse_events, &mouse_event_count);

    for (uint i = 0; i < kbd_event_count; i++) {
      DBG_V("xmit key %s: [%d] 0x%04x\n", kbd_events[i].down ? "DOWN" : "UP", kbd_events[i].page, kbd_events[i].keycode);
      host->kbd_event(kbd_events[i]);
    }

    for (uint i = 0; i < mouse_event_count; i++) {
      host->mouse_event(mouse_events[i]);
    }

    host->update();

    tud_task();
  }
}

void usb_host_setup()
{
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pinout = PIO_USB_PINOUT_DMDP;
  pio_cfg.pin_dp = USB_AUX_DP_GPIO;

  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

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

void enqueue_kbd_event(const KeyboardEvent* event)
{
  //DBG_VV("Enqueued key %s: [%d] 0x%04x\n", event->down ? "DOWN" : "UP", event->page, event->keycode);
  mutex_enter_blocking(&event_queue_mutex);
  if (kbd_event_queue_count < MAX_QUEUED_EVENTS) {
    kbd_event_queue[kbd_event_queue_count++] = *event;
  }
  mutex_exit(&event_queue_mutex);
}

void enqueue_mouse_event(const MouseEvent* event)
{
  //DBG("Enqueued mouse\n");
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
