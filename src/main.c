#include <glob.h>
#include <sys/cdefs.h>
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
#include "stdio_nusb/stdio_usb.h"

#define DEBUG_VERBOSE 0
#define DEBUG_TAG "main"

#include "babelfish.h"

// Whether to run USB host on core1
#define USB_ON_CORE1 1

HOST_PROTOTYPES(sun);
HOST_PROTOTYPES(adb);
HOST_PROTOTYPES(apollo);
HOST_PROTOTYPES(test_3v3);

HostDevice hosts[] = {
  HOST_ENTRY(sun, "Sun emulation. Ch A RX/TX for keyboard, Ch B TX for mouse. Shifter setting 5V."),
  HOST_ENTRY(adb, "ADB emulation. Ch A RX bidirectional. Shifter setting 5V."),
  HOST_ENTRY(apollo, "Apollo emulation. Ch A RX/TX for keyboard and mouse. Shifter setting 5V."),
  HOST_ENTRY(test_3v3, "3v3 TTL test. Transmits A on Ch A TX and B on Ch B TX every 0.5s, 1200 baud 8n1."),
  { 0 }
};

ChannelConfig channels[NUM_CHANNELS] = {
  {
    .channel_num = 0,
    .uart_num = 0,
    .tx_gpio = TX_A_GPIO,
    .rx_gpio = RX_A_GPIO,
    .mux_s0_gpio = CH_A_S0_GPIO,
    .mux_s1_gpio = CH_A_S1_GPIO,
  },
  {
    .channel_num = 1,
    .uart_num = 1,
    .tx_gpio = TX_B_GPIO,
    .rx_gpio = RX_B_GPIO,
    .mux_s0_gpio = CH_B_S0_GPIO,
    .mux_s1_gpio = CH_B_S1_GPIO,
  }
};

// TODO read from flash
int g_current_host_index = 3;

HostDevice *host = NULL;
KeyboardEvent kbd_event_queue[MAX_QUEUED_EVENTS];
MouseEvent mouse_event_queue[MAX_QUEUED_EVENTS];
uint8_t kbd_event_queue_count = 0;
uint8_t mouse_event_queue_count = 0;
mutex_t event_queue_mutex;

uint8_t const ascii_to_hid[128][2] = { HID_ASCII_TO_KEYCODE };
uint8_t const hid_to_ascii[128][2] = { HID_KEYCODE_TO_ASCII };

void usb_host_setup(void);
void core1_main(void);

_Noreturn void mainloop(void);
void channel_init(void);
void led_init(void);
void usb_aux_init(void);
bool cmd_process_event(KeyboardEvent ev);

int main(void)
{
  // need 120MHz for USB
  set_sys_clock_khz(120000, true);

  led_init();

  tud_init(0);

  stdio_nusb_init();

  for (int i = 0; i < 10 && !stdio_nusb_connected(); i++) {
    sleep_ms(100);
  }

  sleep_ms(100);

  DEBUG_INIT();

  DBG("==== B A B E L F I S H ====\n");

  usb_aux_init();

  DBG("Enabled AUX USB\n");

  sleep_ms(100);

  channel_init();

  mutex_init(&event_queue_mutex);

  // Initialize Core 1, and put PIO-USB on it with TinyUSB
  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  host = &hosts[g_current_host_index];

  DBG("Selecting host '%s'\n", host->name);
  DBG("%s\n", host->notes);

  // TODO: read hostid from storage
  host->init();

  mainloop();

  return 0;
}

_Noreturn void mainloop(void)
{
  KeyboardEvent kbd_events[MAX_QUEUED_EVENTS];
  MouseEvent mouse_events[MAX_QUEUED_EVENTS];
  uint kbd_event_count = 0;
  uint mouse_event_count = 0;

  while (true) {
    DEBUG_TASK();

    get_queued_kbd_events(kbd_events, &kbd_event_count);
    get_queued_mouse_events(mouse_events, &mouse_event_count);

    for (uint i = 0; i < kbd_event_count; i++) {
      DBG_V("xmit key %s: [%d] 0x%04x\n", kbd_events[i].down ? "DOWN" : "UP", kbd_events[i].page, kbd_events[i].keycode);
      // if cmd_process_event took the event
      if (cmd_process_event(kbd_events[i]))
        continue;
      host->kbd_event(kbd_events[i]);
    }

    for (uint i = 0; i < mouse_event_count; i++) {
      host->mouse_event(mouse_events[i]);
    }

    host->update();

    gpio_put(LED_P_OK_GPIO, !gpio_get(USB_5V_STAT_GPIO));
    //gpio_put(LED_AUX_GPIO, tud_cdc_connected());
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
