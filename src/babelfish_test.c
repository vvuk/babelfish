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

void config_out_gpio(uint gpio)
{
  gpio_set_function(gpio, GPIO_FUNC_SIO);
  gpio_set_dir(gpio, GPIO_OUT);
  gpio_set_pulls(gpio, true, true); // weak puls
  gpio_put(gpio, 0);
}

void set_a_s(uint val)
{
  gpio_put(CH_A_S0_GPIO, val & 1);
  gpio_put(CH_A_S1_GPIO, (val >> 1) & 1);
}

void set_aux_s(uint val)
{
  gpio_put(13, val & 1);
  gpio_put(14, (val >> 1) & 1);
}

void set_leds(bool a, bool b)
{
  gpio_put(LED_P_OK_GPIO, a);
  gpio_put(LED_AUX_GPIO, b);
}

int main()
{
  // need 120MHz for USB
  set_sys_clock_khz(120000, true);

  DEBUG_INIT();

  led_init();

  config_out_gpio(TX_A_GPIO);
  config_out_gpio(TX_B_GPIO);
  config_out_gpio(CH_A_S0_GPIO);
  config_out_gpio(CH_A_S1_GPIO);

  config_out_gpio(13);
  config_out_gpio(14);

  DBG("===== Test program\n");

  set_leds(gpio_get(CH_A_S0_GPIO), gpio_get(CH_A_S1_GPIO));

  //uart_init(uart0, 2400);

  int ch_config = 0;
  absolute_time_t until = make_timeout_time_ms(0);
  absolute_time_t toggle_t = make_timeout_time_ms(0);
  bool tx = true;

  while (true) {
    DEBUG_TASK();

    if (time_reached(until)) {
      until = make_timeout_time_ms(500);
      set_a_s(ch_config);
      set_aux_s(ch_config);
      set_leds(gpio_get(CH_A_S0_GPIO), gpio_get(CH_A_S1_GPIO));

      ch_config = (ch_config + 1) % 4;
    }

    if (time_reached(toggle_t)) {
      toggle_t = make_timeout_time_ms(20);
      gpio_put(TX_A_GPIO, tx);
      gpio_put(TX_B_GPIO, tx);

      tx = !tx;
    }
  }
}
