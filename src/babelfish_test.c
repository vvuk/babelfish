#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <tusb.h>
#include <stdarg.h>
#include <pio_usb.h>
#include "stdio_nusb/stdio_usb.h"

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

void set_ch_out(uint val)
{
  gpio_put(CH_A_S0_GPIO, val & 1);
  gpio_put(CH_A_S1_GPIO, (val >> 1) & 1);
  gpio_put(CH_B_S0_GPIO, val & 1);
  gpio_put(CH_B_S1_GPIO, (val >> 1) & 1);
}

void set_leds(bool a, bool b)
{
  gpio_put(LED_P_OK_GPIO, a);
  gpio_put(LED_AUX_GPIO, b);
}

static bool s_have_cmd_chars = false;
void usb_debug_chars_avail_cb(void* val)
{
    s_have_cmd_chars = true;
}

const uint32_t wave_interval_us = 500000; // 500ms (2Hz)

_Noreturn void mainloop()
{
    uint32_t next_swap_time = time_us_32() + wave_interval_us;
    char out_char = 'A';

    while (true) {
        uint32_t now = time_us_32();
        if (now >= next_swap_time) {
            uart_putc_raw(uart0, out_char);
            uart_putc_raw(uart1, out_char);
            if (out_char == 'Z') {
                out_char = 'A';
            } else {
                out_char++;
            }

            next_swap_time += wave_interval_us;
        }

        while (uart_is_readable(uart0)) {
            char c = uart_getc(uart0);
            printf("uart0: %c\n", c);
        }
        while (uart_is_readable(uart1)) {
            char c = uart_getc(uart1);
            printf("uart1: %c\n", c);
        }

        while (s_have_cmd_chars) {
            s_have_cmd_chars = false;
            char c = getchar();
            if (c >= '0' && c <= '3') {
                set_ch_out(c - '0');
                printf("output selected: %c\n", c);
            } else if (c == '\n') {
                printf("hardware test: enter 0 1 2 3 to select channel 0 1 2 3\n");
            }
        }
    }
}

int main()
{
  // need 120MHz for USB
  set_sys_clock_khz(120000, true);

  stdio_nusb_init();

  uint32_t until = time_us_32() + 5000000; // 5s for USB
  while (!stdio_nusb_connected() && time_us_32() < until) {}
  stdio_set_chars_available_callback(usb_debug_chars_avail_cb, NULL);

  DEBUG_INIT();

  led_init();

  gpio_set_function(TX_A_GPIO, GPIO_FUNC_UART);
  uart_init(uart0, 1200);

  gpio_set_function(TX_B_GPIO, GPIO_FUNC_UART);
  uart_init(uart1, 1200);

  config_out_gpio(CH_A_S0_GPIO);
  config_out_gpio(CH_A_S1_GPIO);
  config_out_gpio(CH_B_S0_GPIO);
  config_out_gpio(CH_B_S1_GPIO);


  DBG("===== Test program\n");

  mainloop();
}
