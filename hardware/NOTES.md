Babelfish
=========

# v0.3

Fixes from v0.2:
- CH_B_S0/S1 routed to GPIO12 and GPIO13
- exposed DB9 GPIO moved from GPIO13+14 to 14+15.
  - DB9 pin 4 became GPIO15, pin 5 became GPIO14

| GPIO # | Usage |
| ====== | ===== |
|  0 | TX_A |
|  1 | RX_A |
|  2 | CH_A_S0 - A channel output select |
|  3 | CH_A_S1 - A channel output select |
|  6 | LED_PWR |
|  8 | TX_B |
|  9 | RX_B |
| 10 | LED_P_OK |
| 11 | LED_AUX |
| 12 | CH_B_S0 |
| 13 | CH_B_S1 |
| 14 | GPIO14 |
| 15 | GPIO15 |
| 18 | USB2_D- |
| 19 | USB2_D+ |
| 20 | USB2_EN |
| 21 | USB2_FLG |
| 23 | STAT_5V0 |



# v0.2

TPMCP1703T-5002E/CB -- SOT-23-3 20V max input, 300mA max current. How will this play with 357mA current limit on TPS2113? And will this be enough to power USB and RP2040?
TPMCP1703T-3302E/CB - SOT-23-3 


DE9 connector. Female.

5   4   3   2   1
  9   8   7   6

1 - GND
2 - TX A
3 - TX B
4 - GPIO15
5 - GPIO14
6 - VCC_IN
7 - RX A
8 - RX B
9

# Legacy notes
## v01

debug connector:

3v3 SWCLK GND SWD

for power series, top pin is to feed lines

usb2
DP = GPIO19, pin 30
DM = GPIO18, pin 29


pin 1 - U0 TX  - GP0   A1
pin 2 - U0 RX - GP1   A2

pin 6 - U1 TX - GP4   A3
pin 7 - U1 RX - GP5   A4

U1 TX - GP8
U1 RX - GP9

U0 TX - GP12
U0 RX - GP13

U0 TX - GP16
U0 RX - GP17
