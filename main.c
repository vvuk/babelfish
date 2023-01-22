/*
 * USB2Sun
 *  Connect a USB keyboard and mouse to your Sun workstation!
 *
 * Joakim L. Gilje
 * Adapted from the TinyUSB Host examples
 */

#include <bsp/board.h>
#include <tusb.h>

#include "host.h"

HOST_PROTOTYPES(sun);

static HostDevice hosts[] = {
  HOST_ENTRY(sun),
  { 0 }
};

// TODO read from flash
static int g_current_host_index = 0;

HostDevice *host = NULL;

void led_blinking_task(void);

int main(void) {
  board_init();
  tusb_init();

  host = &hosts[g_current_host_index];

  // TODO: read hostid from storage
  host->init();

  while (true) {
    tuh_task();
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
