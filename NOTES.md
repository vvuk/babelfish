
NeXT

0xc5 0xde 0x00 0x00 0x00 -- I think this is an address set? Should be 0xef?
0xc5 0x00 0x06 0x00 0x00 -- Hmm this should be 0x3, this is wrong?

many 0xff = reset

seen on MOUT:
0xc5 0x00 0x03 0x00 0x00 -- 0xC5 -- kbhi = 0x00, kblo = 0x03  -- turn both LEDs on
longish delay
0xc5 0x00 0x00 0x00 0x00  -- turn both LEDs off
delay
0xc6 0x01 0xff 0xff 0xf6 -- 0xc6 is supposed to be an asic-to-computer response?

kbhi = R2 R1 R0 RW x x x A0 ; 0x00 = register 0, write 0, data 0x3

Host 0xc5 0xf0 xx xx xx -- read keyb version, or 0xf1 read mouse version
Reply 0xc6 paddr asicrev devver devid

We should reply
  0xc6
  0b00010000 -- master keyboard, asic originated event
  0x00 -- old asic version
  msb -- metakey code
  lsb -- keycode

-- rom Monitor -- right command key + ~ key on numeric keypad


From Previous:

-> C5 EF000000  -> set addr to 0; should ignore
<- C6 70000000  -> is this some kind of ack?

-> C5 00030000  -> turn on left/right LEDs
<- C6 70000000

-> C5 00000000  -> turn off LEDs
<- C6 70000000

-> C6 01fffff6 -> ... uhh
-> 00
"Command read 0x00"

-> C6 01fffff6 -> Set keyboard mouse poll -- mask & speed, ahh
-> 00




Apollo

Keyboard log after booting domain_os:

```
[   50866] IN: 00 (bytes: 0 cmd: 00000000 reading_cmd: 0)
Unknown keyboard command start byte [not-in-cmd]: 00
[   57317] IN: ff (bytes: 0 cmd: 00000000 reading_cmd: 0)
[   57319] IN: 00 (bytes: 0 cmd: 00000000 reading_cmd: 1)
Keyboard command: 00000000
Got 00000000 command with 0 bytes
[   63278] IN: ff (bytes: 0 cmd: 00000000 reading_cmd: 0)
[   65254] IN: 00 (bytes: 0 cmd: 00000000 reading_cmd: 1)
Keyboard command: 00000000
Got 00000000 command with 0 bytes
[   65262] IN: ff (bytes: 0 cmd: 00000000 reading_cmd: 0)
[   65267] IN: 12 (bytes: 0 cmd: 00000000 reading_cmd: 1)
[   65272] IN: 21 (bytes: 1 cmd: 00000012 reading_cmd: 1)
```

i53 / 15 / 1637

ST15150N: 78 / 21 / 5119


after booting domain_os; responses resulted in junk answers to "do yo uawnt to run domain_os with current calendar" prompt:

(apollo:0) recv ff
(apollo:0) recv 00
(apollo:0)  command 00000000 (1 bytes)
(apollo:0) Setting keyboard mode to 0
(apollo:0) xmit ff
(apollo:0) xmit 00
(apollo:0)  command handled
(apollo:0) recv ff
(apollo:0) recv 00
(apollo:0)  command 00000000 (1 bytes)
(apollo:0) Setting keyboard mode to 0
(apollo:0) xmit ff
(apollo:0) xmit 00
(apollo:0)  command handled
(apollo:0) recv ff
(apollo:0) recv 00
(apollo:0)  command 00000000 (1 bytes)
(apollo:0) Setting keyboard mode to 0
(apollo:0) xmit ff
(apollo:0) xmit 00
(apollo:0)  command handled
(apollo:0) recv ff
(apollo:0) recv 12
(apollo:0)  command 00000012 (1 bytes)
(apollo:0) recv 21
[started sending here]

booting the os --

(apollo:0) recv ff
(apollo:0) recv 00
(apollo:0)  command 00000000 (1 bytes)
(apollo:0) Setting keyboard mode to 0
(apollo:0) xmit ff
(apollo:0) xmit 00
(apollo:0)  command handled
(apollo:0) recv ff
(apollo:0) recv 01
(apollo:0)  command 00000001 (1 bytes)
(apollo:0) Setting keyboard mode to 1
(apollo:0) xmit ff
(apollo:0) xmit 01
(apollo:0)  command handled
(apollo:0) recv ff
(apollo:0) recv 12
(apollo:0)  command 00000012 (1 bytes)
(apollo:0) recv 21
(apollo:0)  command 00001221 (2 bytes)
(apollo:0) keyboard ident request
'D-03863-MSxmit str '3-@
(apollo:0)  command handled
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00
(apollo:0) recv 00


resulted in uqyqr\qru]puqk\ in the login prompt

upon using the mouse, the keyboard keys seem to be sending mouse input?

