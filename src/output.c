#define DEBUG_TAG "output"
#include "debug.h"
#include "babelfish.h"

void channel_init() {
  for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
    ChannelConfig *cfg = &channels[ch];
    gpio_set_function(cfg->tx_gpio, GPIO_FUNC_SIO);
    gpio_set_function(cfg->rx_gpio, GPIO_FUNC_SIO);
    gpio_set_function(cfg->mux_s0_gpio, GPIO_FUNC_SIO);
    gpio_set_function(cfg->mux_s1_gpio, GPIO_FUNC_SIO);
    gpio_set_dir(cfg->tx_gpio, GPIO_OUT);
    gpio_set_dir(cfg->rx_gpio, GPIO_IN);
    gpio_set_dir(cfg->mux_s0_gpio, GPIO_OUT);
    gpio_set_dir(cfg->mux_s1_gpio, GPIO_OUT);
    gpio_put(cfg->mux_s0_gpio, 0);
    gpio_put(cfg->mux_s1_gpio, 0);
  }
}

void channel_config(int ch, ChannelMode mode) {
  DBG("Channel %c set config: 0x%08x\n", 'A' + ch, mode);

  ChannelConfig *cfg = &channels[ch];
  if (cfg->mode == mode)
    return;

  switch (mode & ChannelModeOutputTypeMask) {
    case ChannelModeGPIO:
      gpio_set_function(cfg->tx_gpio, GPIO_FUNC_SIO);
      gpio_set_function(cfg->rx_gpio, GPIO_FUNC_SIO);
      break;
    case ChannelModeUART:
      gpio_set_function(cfg->tx_gpio, GPIO_FUNC_UART);
      gpio_set_function(cfg->rx_gpio, GPIO_FUNC_UART);
      break;
  }

  switch (mode & ChannelModeInvertMask) {
    case ChannelModeNoInvert:
      gpio_set_inover(cfg->rx_gpio, GPIO_OVERRIDE_NORMAL);
      gpio_set_outover(cfg->tx_gpio, GPIO_OVERRIDE_NORMAL);
      break;
    case ChannelModeInvert:
      gpio_set_inover(cfg->rx_gpio, GPIO_OVERRIDE_INVERT);
      gpio_set_outover(cfg->tx_gpio, GPIO_OVERRIDE_INVERT);
      break;
  }

  uint8_t mux_mode = mode & ChannelModeOutputMask;
  gpio_put(cfg->mux_s0_gpio, mux_mode & 1);
  gpio_put(cfg->mux_s1_gpio, (mux_mode >> 1) & 1);

  cfg->mode = mode;
}
