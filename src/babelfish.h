#ifndef BABLEFISH_H_
#define BABELFISH_H_

#include "host.h"

/*
 * The "A" GPIO pins are directly connected from the rp2040.
 * They will use 3v3 signaling.  These pins can be configured
 * for UART0 or UART1 as well.
 */
#define U0_TX_A_GPIO 0
#define U0_RX_A_GPIO 1
#define U1_TX_A_GPIO 4
#define U1_RX_A_GPIO 5

/*
 * The "B" GPIO pins are connected to a transistor level shifter.
 * The level is set via jumpers, and can be 3v3, 5v, or VCC in
 * from the target computer. These pins can also be configured
 * for UART0 or UART1.
 */
#define U0_TX_B_GPIO 12
#define U0_RX_B_GPIO 13
#define U1_TX_B_GPIO 8
#define U1_RX_B_GPIO 9

/*
 * Babelfish v0.2 has two LEDs connected on these two GPIOs,
 * nominally used for power and for an OK indicator.
 */
#define LED_PWR_GPIO 6
#define LED_P_OK_GPIO 10

/*
 * One AUX gpio is broken out.
 */
#define LED_AUX_GPIO 11

/*
 * The secondary USB port has D- on GPIO 18 and D+ on GPIO 19.
 */
#define USB_AUX_DM_GPIO 18
#define USB_AUX_DP_GPIO 19

#endif
