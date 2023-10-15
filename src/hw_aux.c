#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <tusb.h>
#include <pio_usb.h>

#define DEBUG_VERBOSE 0
#define DEBUG_TAG "hwaux"

#include "babelfish.h"

void usb_pwr_signal_irq(uint gpio, uint32_t event_mask)
{
    bool stat_ok = !gpio_get(USB_5V_STAT_GPIO);
    bool aux_ok = !gpio_get(USB_AUX_EN_GPIO);
    bool ok = stat_ok && aux_ok;

    gpio_put(LED_P_OK_GPIO, ok);

    if (gpio != 0)
        gpio_acknowledge_irq(gpio, event_mask);
}

void usb_aux_init(void)
{
    gpio_set_dir(USB_AUX_EN_GPIO, GPIO_OUT);
    gpio_set_function(USB_AUX_EN_GPIO, GPIO_FUNC_SIO);
    gpio_put(USB_AUX_EN_GPIO, 1);

    gpio_set_dir(USB_5V_STAT_GPIO, GPIO_IN);
    gpio_set_function(USB_5V_STAT_GPIO, GPIO_FUNC_SIO);

    gpio_set_irq_enabled_with_callback(USB_5V_STAT_GPIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
            true, usb_pwr_signal_irq);
    gpio_set_irq_enabled_with_callback(USB_AUX_EN_GPIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
            true, usb_pwr_signal_irq);

    // trigger it once manually to set the current state
    usb_pwr_signal_irq(0, 0);
}

void led_init(void)
{
    uint8_t leds[] = { LED_PWR_GPIO, LED_P_OK_GPIO, LED_AUX_GPIO };

    for (uint i = 0; i < sizeof(leds); i++) {
        gpio_set_drive_strength(leds[i], GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_dir(leds[i], GPIO_OUT);
        gpio_set_function(leds[i], GPIO_FUNC_SIO);
        gpio_put(leds[i], 1);
    }

    sleep_ms(100);
    gpio_put(LED_P_OK_GPIO, 0);
    gpio_put(LED_AUX_GPIO, 0);
}

