#include <pico/stdlib.h>

#include "babelfish.h"

void babelfish_uart_config(int uidx, char ab) {
  int tx_gpio;
  int rx_gpio;

  if (uidx == 0) {
    gpio_set_function(U0_TX_A_GPIO, GPIO_FUNC_NULL);
    gpio_set_function(U0_RX_A_GPIO, GPIO_FUNC_NULL);
    gpio_set_function(U0_TX_B_GPIO, GPIO_FUNC_NULL);
    gpio_set_function(U0_RX_B_GPIO, GPIO_FUNC_NULL);

    tx_gpio = ab == 'a' ? U0_TX_A_GPIO : U0_TX_B_GPIO;
    rx_gpio = ab == 'a' ? U0_RX_A_GPIO : U0_RX_B_GPIO;
  } else if (uidx == 1) {
    gpio_set_function(U1_TX_A_GPIO, GPIO_FUNC_NULL);
    gpio_set_function(U1_RX_A_GPIO, GPIO_FUNC_NULL);
    gpio_set_function(U1_TX_B_GPIO, GPIO_FUNC_NULL);
    gpio_set_function(U1_RX_B_GPIO, GPIO_FUNC_NULL);

    tx_gpio = ab == 'a' ? U1_TX_A_GPIO : U1_TX_B_GPIO;
    rx_gpio = ab == 'a' ? U1_RX_A_GPIO : U1_RX_B_GPIO;
  } else {
    return;
  }

  gpio_set_function(tx_gpio, GPIO_FUNC_UART);
  gpio_set_function(rx_gpio, GPIO_FUNC_UART);
  gpio_set_inover(rx_gpio, GPIO_OVERRIDE_NORMAL);
  gpio_set_outover(tx_gpio, GPIO_OVERRIDE_NORMAL);
}