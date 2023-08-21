#include <pico/stdlib.h>
#include <bsp/board.h>

#include "babelfish.h"

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