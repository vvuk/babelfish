#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <tusb.h>
#include <stdarg.h>
#include <pio_usb.h>

#define DEBUG_TAG "test"
#include "babelfish.h"

uint8_t leds[] = { LED_PWR_GPIO, LED_P_OK_GPIO, LED_AUX_GPIO };

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

void led_init(void);
void channel_init(void);

int main()
{
  // need 120MHz for USB
  set_sys_clock_khz(120000, true);

  led_init();
  channel_init();

  channel_config(0, ChannelModeGPIO | ChannelModeNoInvert | ChannelModeDirect);

  gpio_set_function(13, GPIO_FUNC_SIO);
  gpio_set_function(14, GPIO_FUNC_SIO);
  gpio_set_dir(13, GPIO_OUT);
  gpio_set_dir(14, GPIO_OUT);

  gpio_put(CH_A_S0_GPIO, 1);
  gpio_put(CH_A_S1_GPIO, 0);

  DEBUG_INIT();

  DBG("===== Test program\n");

  while (true) {
    DEBUG_TASK();

    gpio_put(TX_A_GPIO, 1);
    gpio_put(TX_B_GPIO, 1);
    gpio_put(13, 1);
    gpio_put(14, 0);
    sleep_ms(100);
    gpio_put(TX_A_GPIO, 0);
    gpio_put(TX_B_GPIO, 0);
    gpio_put(13, 0);
    gpio_put(14, 1);
    sleep_ms(100);
  }
}
