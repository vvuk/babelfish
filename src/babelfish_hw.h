#ifndef BABLEFISH_H_
#define BABELFISH_H_

// Channel A pins
#define TX_A_GPIO 0
#define RX_A_GPIO 1

// Mux selection for channel A.
//
// 0 = direct
// 1 = via level shifter
// 2 = via MAX3232
#define CH_A_S0_GPIO 2
#define CH_A_S1_GPIO 3

// Channel B pins
#define TX_B_GPIO 8
#define RX_B_GPIO 9

// Mux selection for channel B.
// v0.2 these are floating unless they are patched
// v0.3 only -- these were not connected on v0.2, oops.
#define CH_B_S0_GPIO 12
#define CH_B_S1_GPIO 13

// Babelfish v0.2 has LEDs on these three GPIOs,
// typically used for power indications and an aux indicator.
#define LED_PWR_GPIO 6
#define LED_P_OK_GPIO 10
#define LED_AUX_GPIO 11

// The secondary USB port has D- on GPIO 18 and D+ on GPIO 19.
#define USB_AUX_DM_GPIO 18
#define USB_AUX_DP_GPIO 19

// The USB 5V controller breaks out a status signal, connected to GPIO 23.
#define USB_AUX_EN_GPIO 20
#define USB_AUX_FLG_GPIO 21
#define USB_5V_STAT_GPIO 23

typedef enum {
    ChannelModeDirect = 0,
    ChannelModeLevelShifter = 1,
    ChannelMode232 = 2,
    ChannelModeOutputVoltageMask = 0x0f,

    ChannelModeGPIO = 0 << 4, // configure this channel as bare GPIO
    ChannelModeUART = 1 << 4, // configure this channel as a UART
    ChannelModeOutputTypeMask = 0xf0,

    ChannelModeNoInvert = 0 << 8, // don't invert output
    ChannelModeInvert = 1 << 8, // invert output
    ChannelModeInvertMask = 0xf00,
} ChannelMode;

typedef struct ChannelConfig {
    uint8_t channel_num;
    uint8_t uart_num;
    uint8_t tx_gpio;
    uint8_t rx_gpio;
    uint8_t mux_s0_gpio;
    uint8_t mux_s1_gpio;

    ChannelMode mode;
} ChannelConfig;

#define NUM_CHANNELS 2

extern ChannelConfig channels[NUM_CHANNELS];

extern void channel_config(int channel_num, ChannelMode mode);

#endif
