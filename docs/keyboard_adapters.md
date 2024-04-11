# Keyboard Adapters

Babelfish DE9 connector (Female):

```
5   4   3   2   1
  9   8   7   6

1 - GND
2 - TX A
3 - TX B
4 - GPIO14
5 - GPIO15
6 - VCC_5V (5V in, if available)
7 - RX A
8 - RX B
9 - 3v3 (out)
```

## Apollo

CPU end of _*cable*_ (yes, these are the actual DIN8 pin numbers):

```
              U          1 - +8.5 Vdc (see below)     Red wire
          6o    7o       2 - Data to CPU (TXD)        Yellow wire
                         3 - Logic ground             Green wire
        1o        3o     4 - RESET                    Orange wire
             8o          5 - Data to keyboard (RXD)   Grey wire
          4o    5o       6 - Not used
             2o          7 - Logic ground             Blue wire
                         8 - Not used
                    SHIELD - Chassis ground
```

| Babelfish DB9 | Apollo DIN8 |
| :--- | :--- |
| 1 (GND) | 3, 7 (Logic ground) (white/3) |  
| 2 (TX A) | 2 (Data to CPU) (light brown/light orange) |
| 7 (RX A) | 5 (Data to keyboard) (yellow) |
| | 4 (RESET) (dark brown) |

## ADB

(todo)

## Sun (pre-PS/2)

(todo)

## SGI (pre-PS/2)

(todo)
