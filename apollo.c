#include <bsp/board.h>
#include <tusb.h>

#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>

#define UART_KBD_ID uart1
#define UART_KBD_IRQ UART1_IRQ
#define UART_KBD_TX_PIN 4
#define UART_KBD_RX_PIN 5

#define UART_MOUSE_ID uart0
#define UART_MOUSE_TX_PIN 0

typedef enum {
    Mode0_Compatibility,
    Mode1_Keystate,
    Mode2_RelativeCursor,
    Mode3_AbsoluteCursor
} KeyboardMode;

typedef enum {
    State_Down = 0,
    State_Up,
    State_Unshifted,
    State_Shifted,
    State_Control,
    State_CapsLock,
    State_UpTrans,
    State_AutoRepeat,
    StateMax
} KeyState;

static KeyboardMode kbd_mode = Mode0_Compatibility;

static void on_keyboard_rx();

// defined at end of file
static uint16_t s_code_table[256][StateMax];

void apollo_init() {
  gpio_set_function(UART_KBD_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_KBD_RX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_MOUSE_TX_PIN, GPIO_FUNC_UART);

  uart_init(UART_KBD_ID, 1200);
  uart_set_hw_flow(UART_KBD_ID, false, false);
  uart_set_format(UART_KBD_ID, 8, 1, UART_PARITY_NONE);

  uart_init(UART_MOUSE_ID, 1200);
  uart_set_hw_flow(UART_MOUSE_ID, false, false);
  uart_set_format(UART_MOUSE_ID, 8, 1, UART_PARITY_NONE);

  irq_set_exclusive_handler(UART_KBD_IRQ, on_keyboard_rx);
  irq_set_enabled(UART_KBD_IRQ, true);

  uart_set_irq_enables(UART_KBD_ID, true, false);

  gpio_set_inover(UART_KBD_RX_PIN, GPIO_OVERRIDE_INVERT);
  gpio_set_outover(UART_KBD_TX_PIN, GPIO_OVERRIDE_INVERT);
  gpio_set_outover(UART_MOUSE_TX_PIN, GPIO_OVERRIDE_INVERT);
}

void apollo_update() {
}


void apollo_kbd_report(hid_keyboard_report_t const *report) {
    static bool down_state[256] = {false};

    uint8_t up_keys[6]; // up to 6 keys could have been released this frame
    uint8_t up_key_count = 0;

    KeyState selector = State_Unshifted;

    bool ctrl =  (report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)) != 0;
    bool shift = (report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0;
    //bool caps = 
    //bool repeat =

    // find any released keys.  this is a silly loop.
    for (int hidk = 1; hidk < 256; hidk++) {
        if (!down_state[hidk])
            continue;
        for (int j = 0; j < 6; j++) {
            if (report->keycode[j] == hidk) {
                down_state[hidk] = false;
                up_keys[up_key_count++] = hidk;
                break;
            }
        }
    }

    // we're only going to report on the first one, though we have to track down_state for all
    uint8_t hidcode = report->keycode[0];

    if (kbd_mode == Mode0_Compatibility) {
        uint16_t code = 0;

        if (ctrl)
            code = s_code_table[hidcode][State_Control];
        else if (shift)
            code = s_code_table[hidcode][State_Shifted];
        else
            code = s_code_table[hidcode][State_Unshifted];
        
        if (code != 0) {
            printf("Translating %02x to %04x\n", hidcode, code);
            uart_putc(UART_KBD_ID, code);
        }
    } else {
        // assume that Mode 1, 2, 3 all want key state
        // TODO
        // TODO mode1 will want us to translate modifier keys to up/down of that key as well
    }
}

void apollo_mouse_report(hid_mouse_report_t const *report) {
}

void on_keyboard_rx() {
    static uint32_t kbd_cmd = 0;
    static bool kbd_reading_cmd = false;
    static int kbd_cmd_bytes = 0;

    bool cmd_handled;

    while (uart_is_readable(UART_KBD_ID)) {
        uint8_t ch = uart_getc(UART_KBD_ID);

        if (!kbd_reading_cmd) {
            if (ch == 0xff) {
                kbd_reading_cmd = true;
                kbd_cmd = 0;
                kbd_cmd_bytes = 0;
            } else {
                printf("Unknown keyboard command start byte [not-in-cmd]: %02x\n", ch);
            }
        } else {
            if (ch == 0x00) {
                kbd_reading_cmd = false;
            } else {
                if (kbd_cmd_bytes == 4) {
                    printf("Too-long keyboard command [in-cmd]: %08x, got %02x\n", kbd_cmd, ch);
                    kbd_reading_cmd = false;
                    continue;
                }
                kbd_cmd = (kbd_cmd << 8) | ch;
                kbd_cmd_bytes++;
            }
        }

        // If we're still reading a command (didn't get a 0x00 byte), continue doing so
        if (kbd_reading_cmd) 
            continue;

        printf("Keyboard command: %08x\n", kbd_cmd);

        cmd_handled = true;
        switch (kbd_cmd) {
            case 0x00:
                if (kbd_cmd_bytes != 1) { printf("Got %08x command with %d bytes\n", kbd_cmd, kbd_cmd_bytes); }
                kbd_mode = Mode0_Compatibility;
                break;
            
            case 0x01:
                if (kbd_cmd_bytes != 1) { printf("Got %08x command with %d bytes\n", kbd_cmd, kbd_cmd_bytes); }
                kbd_mode = Mode1_Keystate;
                break;

            case 0x2181: // beeper on for 300ms
            case 0x2182: // beeper off
            case 0x1221: // keyboard identification
                // english: "3-@\r2-0\rSD-03863-MS\r"
                // german:  "3-A\r2-0\rSD-03863-MS\r"
                // this code also used to either stay in Mode0, or reset to Mode1
            case 0x1166: // unknown
            case 0x1117: // unknown
                printf("Unhandled\n");
                cmd_handled = false;
        }

        if (cmd_handled) {
            // ASSUMPTION: only loopback commands that were handled?
            uart_putc(UART_KBD_ID, 0xff);
            for (int i = 0; i < kbd_cmd_bytes; i++) {
                uart_putc(UART_KBD_ID, (kbd_cmd >> (8 * (kbd_cmd_bytes - i - 1))) & 0xff);
            }
            // original MAME code did not loopback the 0x00
            uart_putc(UART_KBD_ID, 0x00);
        }
    }
}

#define Yes 1
#define No 0
#define NOP 0

static uint16_t s_code_table[256][StateMax] = {
		                          /* MAME? | Keycap         | Down | Up  |Unshifted|Shifted|Control|Caps Lock|Up Trans|Auto  */
		                          /* Number| Legend         | Code | Code|Code     | Code  | Code  |Code     | Code   |Repeat*/

		[HID_KEY_GRAVE]         = /* B14     ~ '         */ { 0x24, 0xA4, 0x60,     0x7E,   0x1E,   0x60,     NOP,     No  },
		[HID_KEY_ESCAPE]        = /* B1      ESC         */ { 0x17, 0x97, 0x1B,     0x1B,   NOP,    0x1B,     NOP,     No  },
		[HID_KEY_1]             = /* B2      ! 1         */ { 0x18, 0x98, 0x31,     0x21,   NOP,    0x31,     NOP,     No  },
		[HID_KEY_2]             = /* B3      @ 2         */ { 0x19, 0x99, 0x32,     0x40,   NOP,    0x32,     NOP,     No  },
		[HID_KEY_3]             = /* B4      # 3         */ { 0x1A, 0x9A, 0x33,     0x23,   NOP,    0x33,     NOP,     No  },
		[HID_KEY_4]             = /* B5      $ 4         */ { 0x1B, 0x9B, 0x34,     0x24,   NOP,    0x34,     NOP,     No  },
		[HID_KEY_5]             = /* B6      % 5         */ { 0x1C, 0x9C, 0x35,     0x25,   NOP,    0x35,     NOP,     No  },
		[HID_KEY_6]             = /* B7      ^ 6         */ { 0x1D, 0x9D, 0x36,     0x5E,   NOP,    0x36,     NOP,     No  },
		[HID_KEY_7]             = /* B8      & 7         */ { 0x1E, 0x9E, 0x37,     0x26,   NOP,    0x37,     NOP,     No  },
		[HID_KEY_8]             = /* B9      * 8         */ { 0x1F, 0x9F, 0x38,     0x2A,   NOP,    0x38,     NOP,     No  },
		[HID_KEY_9]             = /* B10     ( 9         */ { 0x20, 0xA0, 0x39,     0x28,   NOP,    0x39,     NOP,     No  },
		[HID_KEY_0]             = /* B11     ) 0         */ { 0x21, 0xA1, 0x30,     0x29,   NOP,    0x30,     NOP,     No  },
		[HID_KEY_MINUS]         = /* B12     _ -         */ { 0x22, 0xA2, 0x2D,     0x5F,   NOP,    0x2D,     NOP,     Yes },
		[HID_KEY_EQUAL]         = /* B13     + =         */ { 0x23, 0xA3, 0x3D,     0x2B,   NOP,    0x3D,     NOP,     Yes },
		[HID_KEY_BACKSLASH]     = /* D14     \\ |        */ { 0x53, 0xD3, 0xC8,     0xC9,   NOP,    0xC8,     NOP,     No  },
		[HID_KEY_BACKSPACE]     = /* B15     BACKSPACE   */ { 0x25, 0xA5, 0x5E,     0x5E/*0xDE, 0xDE*/,   NOP,    0xDE,     NOP,     Yes },

		[HID_KEY_TAB] = /* C1      TAB         */ { 0x2C, 0xAC, 0xCA,     0xDA,   0xFA,   0xCA,     NOP,     No  },
		[HID_KEY_Q] = /* C2      Q           */ { 0x2D, 0xAD, 0x71,     0x51,   0x11,   0x51,     NOP,     No  },
		[HID_KEY_W] = /* C3      W           */ { 0x2E, 0xAE, 0x77,     0x57,   0x17,   0x57,     NOP,     No  },
		[HID_KEY_E] = /* C4      E           */ { 0x2F, 0xAF, 0x65,     0x45,   0x05,   0x45,     NOP,     No  },
		[HID_KEY_R] = /* C5      R           */ { 0x30, 0xB0, 0x72,     0x52,   0x12,   0x52,     NOP,     No  },
		[HID_KEY_T] = /* C6      T           */ { 0x31, 0xB1, 0x74,     0x54,   0x14,   0x54,     NOP,     No  },
		[HID_KEY_Y] = /* C7      Y           */ { 0x32, 0xB2, 0x59,     0x59,   0x19,   0x59,     NOP,     No  },
		[HID_KEY_U] = /* C8      U           */ { 0x33, 0xB3, 0x75,     0x55,   0x15,   0x55,     NOP,     No  },
		[HID_KEY_I] = /* C9      I           */ { 0x34, 0xB4, 0x69,     0x49,   0x09,   0x49,     NOP,     No  },
		[HID_KEY_O] = /* C10     O           */ { 0x35, 0xB5, 0x6F,     0x4F,   0x0F,   0x4F,     NOP,     No  },
		[HID_KEY_P] = /* C11     P           */ { 0x36, 0xB6, 0x70,     0x50,   0x10,   0x50,     NOP,     No  },
		[HID_KEY_BRACKET_LEFT] = /* C12     { [ / Ue    */ { 0x37, 0xB7, 0x7B,     0x5B,   0x1B,   0x7B,     NOP,     No  },
		[HID_KEY_BRACKET_RIGHT] = /* C13     } ] / Oe    */ { 0x38, 0xB8, 0x7D,     0x5D,   0x1D,   0x7D,     NOP,     No  },
		                 // /* D13     RETURN      */ { 0x52, 0xD2, 0xCB,     0xDB,   NOP,    0xCB,     NOP,     No  },
		[HID_KEY_ENTER] = /* D13     RETURN      */ { 13, 13, 13,     13,   NOP,    13,     NOP,     No  },

		[HID_KEY_A] = /* D2      A           */ { 0x46, 0xC6, 0x61,     0x41,   0x01,   0x41,     NOP,     No  },
		[HID_KEY_S] = /* D3      S           */ { 0x47, 0xC7, 0x73,     0x53,   0x13,   0x53,     NOP,     No  },
		[HID_KEY_D] = /* D4      D           */ { 0x48, 0xC8, 0x64,     0x44,   0x04,   0x44,     NOP,     No  },
		[HID_KEY_F] = /* D5      F           */ { 0x49, 0xC9, 0x66,     0x46,   0x06,   0x46,     NOP,     No  },
		[HID_KEY_G] = /* D6      G           */ { 0x4A, 0xCA, 0x67,     0x47,   0x07,   0x47,     NOP,     No  },
		[HID_KEY_H] = /* D7      H           */ { 0x4B, 0xCB, 0x68,     0x48,   0x08,   0x48,     NOP,     No  },
		[HID_KEY_J] = /* D8      J           */ { 0x4C, 0xCC, 0x6A,     0x4A,   0x0A,   0x4A,     NOP,     No  },
		[HID_KEY_K] = /* D9      K           */ { 0x4D, 0xCD, 0x6B,     0x4B,   0x0B,   0x4B,     NOP,     No  },
		[HID_KEY_L] = /* D10     L           */ { 0x4E, 0xCE, 0x6C,     0x4C,   0x0C,   0x4C,     NOP,     No  },
		[HID_KEY_SEMICOLON] = /* D11     : ; / Oe    */ { 0x4F, 0xCF, 0x3B,     0x3A,   0xFB,   0x3B,     NOP,     No  },
		[HID_KEY_APOSTROPHE] = /* D12     " ' / Ae    */ { 0x50, 0xD0, 0x27,     0x22,   0xF8,   0x27,     NOP,     No  },
// Apollo US keyboards have no hash key (#)
		//[HID_KEY_] = /* D14     ' #         */ { NOP,  NOP,  0x23,     0x27,   NOP,    0x23,     NOP,     No  },

		[HID_KEY_Z] = /* E2      Z           */ { 0x60, 0xE0, 0x5A,     0x5A,   0x1A,   0x5A,     NOP,     No  },
		[HID_KEY_X] = /* E3      X           */ { 0x61, 0xE1, 0x78,     0x58,   0x18,   0x58,     NOP,     No  },
		[HID_KEY_C] = /* E4      C           */ { 0x62, 0xE2, 0x63,     0x43,   0x03,   0x43,     NOP,     No  },
		[HID_KEY_V] = /* E5      V           */ { 0x63, 0xE3, 0x76,     0x56,   0x16,   0x56,     NOP,     No  },
		[HID_KEY_B] = /* E6      B           */ { 0x64, 0xE4, 0x62,     0x42,   0x02,   0x42,     NOP,     No  },
		[HID_KEY_N] = /* E7      N           */ { 0x65, 0xE5, 0x6E,     0x4E,   0x0E,   0x4E,     NOP,     No  },
		[HID_KEY_M] = /* E8      M           */ { 0x66, 0xE6, 0x6D,     0x4D,   0x0D,   0x4D,     NOP,     No  },
		[HID_KEY_COMMA] = /* E9      < ,         */ { 0x67, 0xE7, 0x2C,     0x3C,   NOP,    0x2C,     NOP,     No  },
		[HID_KEY_PERIOD] = /* E10     > .         */ { 0x68, 0xE8, 0x2E,     0x3E,   NOP,    0x2E,     NOP,     Yes },
		[HID_KEY_SLASH] = /* E11     ? /         */ { 0x69, 0xE9, 0xCC,     0xDC,   0xFC,   0xCC,     NOP,     No  },

		[HID_KEY_SPACE] = /* F1      (space bar) */ { 0x76, 0xF6, 0x20,     0x20,   0x20,   0x20,     NOP,     Yes },
		[HID_KEY_HOME] = /* LC0     Home        */ { 0x27, 0xA7, 0x84,     0x94,   0x84,   0x84,     0xA4,    No  },
#if !MAP_APOLLO_KEYS
		[HID_KEY_DELETE] = /* C14     DELETE      */ { 0x3A, 0xBA, 0x7F,     0x7F,   NOP,    0x7F,     NOP,     Yes },
#else
		[HID_KEY_DELETE] = /* E13     POP         */ { 0x6C, 0xEC, 0x80,     0x90,   0x80,   0x80,     0xA0,    No  },
#endif
		[HID_KEY_PAGE_UP] = /* LF0     Roll Up     */ { 0x72, 0xF2, 0x8D,     0x9D,   0x8D,   0x8D,     0xAD,    No  },
		[HID_KEY_PAGE_DOWN] = /* LF2     Roll Down   */ { 0x74, 0xF4, 0x8F,     0x9F,   0x8F,   0x8F,     0xAF,    No  },
		[HID_KEY_END] = /* LC2     End         */ { 0x29, 0xA9, 0x86,     0x96,   0x86,   0x86,     0xA6,    No  },
		[HID_KEY_ARROW_LEFT] = /* LE0     Cursor left */ { 0x59, 0xD9, 0x8A,     0x9A,   0x9A,   0x9A,     0xAA,    Yes },
		[HID_KEY_ARROW_UP] = /* LD1     Cursor Up   */ { 0x41, 0xC1, 0x88,     0x98,   0x88,   0x88,     0xA8,    Yes },
		[HID_KEY_ARROW_RIGHT] = /* LE2     Cursor right*/ { 0x5B, 0xDB, 0x8C,     0x9C,   0x8C,   0xBE,     0xAC,    Yes },
		[HID_KEY_ARROW_DOWN] = /* LF1     Cursor down */ { 0x73, 0xF3, 0x8E,     0x9E,   0x8E,   0x8E,     0xAE,    Yes },

        // TODO mode1 will want us to translate modifier keys
#if false
		[HID_KEY_] = /* D1      CAPS LOCK   */ { NOP,  NOP,  NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /* E1      SHIFT       */ { 0x5E, 0xDE, NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /* DO      CTRL        */ { 0x43, 0xC3, NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
// FIXME: ALT swapped!
		[HID_KEY_] = /* ??      ALT_R       */ { 0x77, 0xF7, 0xfe00,   NOP,    NOP,    NOP,      0xfe01,  No  },
		[HID_KEY_] = /* ??      ALT_L       */ { 0x75, 0xF5, 0xfe02,   NOP,    NOP,    NOP,      0xfe03,  No  },
		[HID_KEY_] = /* E12     SHIFT       */ { 0x6A, 0xEA, NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
#endif

#if false
#if !MAP_APOLLO_KEYS
		[HID_KEY_] = /*         Numpad CLR  */ { NOP,  NOP,  NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /*         Numpad /    */ { NOP,  NOP,  NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /*         Numpad *    */ { NOP,  NOP,  NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /* RD4     -           */ { 0x58, 0xD8, 0xFE2D,   0xFE5F, NOP,    0xFE2D,   NOP,     No  },
		[HID_KEY_] = /* RC1     7           */ { 0x3C, 0xBC, 0xFE37,   0xFE26, NOP,    0xFE37,   NOP,     No  },
		[HID_KEY_] = /* RC2     8           */ { 0x3D, 0xBD, 0xFE38,   0xFE2A, NOP,    0xFE38,   NOP,     No  },
		[HID_KEY_] = /* RC3     9           */ { 0x3E, 0xBE, 0xFE39,   0xFE28, NOP,    0xFE39,   NOP,     No  },
		[HID_KEY_] = /* RC4     +           */ { 0x3F, 0xBF, 0xFE2B,   0xFE3D, NOP,    0xFE2B,   NOP,     No  },
		[HID_KEY_] = /* RD1     4           */ { 0x55, 0xD5, 0xFE34,   0xFE24, NOP,    0xFE34,   NOP,     No  },
		[HID_KEY_] = /* RD2     5           */ { 0x56, 0xD6, 0xFE35,   0xFE25, NOP,    0xFE35,   NOP,     No  },
		[HID_KEY_] = /* RD3     6           */ { 0x57, 0xD7, 0xFE36,   0xFE5E, NOP,    0xFE36,   NOP,     No  },
		[HID_KEY_] = /*         Numpad =    */ { NOP,  NOP,  NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /* RE1     1           */ { 0x6E, 0xEE, 0xFE31,   0xFE21, NOP,    0xFE31,   NOP,     No  },
		[HID_KEY_] = /* RE2     2           */ { 0x6F, 0xEF, 0xFE32,   0xFE40, NOP,    0xFE32,   NOP,     No  },
		[HID_KEY_] = /* RE3     3           */ { 0x70, 0xF0, 0xFE33,   0xFE23, NOP,    0xFE33,   NOP,     No  },
		[HID_KEY_] = /* RF3     ENTER       */ { 0x7C, 0xFC, 0xFECB,   0xFEDB, NOP,    0xFECB,   NOP,     No  },
		[HID_KEY_] = /* RF1     0           */ { 0x79, 0xF9, 0xFE30,   0xFE29, NOP,    0xFE30,   NOP,     No  },
		[HID_KEY_] = /*         Numpad ,    */ { NOP,  NOP,  NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /* RF2     .           */ { 0x7B, 0xFB, 0xFE2E,   0xFE2E, NOP,    0xFE2E,   NOP,     No  },
		[HID_KEY_] = /* A0      F0          */ { 0x04, 0x84, 0x1C,     0x5C,   0x7C,   0x1C,     0xBC,    No  },
		[HID_KEY_] = /* A1      F1          */ { 0x05, 0x85, 0xC0,     0xD0,   0xF0,   0xC0,     0xE0,    No  },
		[HID_KEY_] = /* A2      F2          */ { 0x06, 0x86, 0xC1,     0x01,   0xF1,   0xC1,     0xE1,    No  },
		[HID_KEY_] = /* A3      F3          */ { 0x07, 0x87, 0xC2,     0x02,   0xF2,   0xC2,     0xE2,    No  },
		[HID_KEY_] = /* A4      F4          */ { 0x08, 0x88, 0xC3,     0x03,   0xF3,   0xC3,     0xE3,    No  },
		[HID_KEY_] = /* A5      F5          */ { 0x09, 0x89, 0xC4,     0x04,   0xF4,   0xC4,     0xE4,    No  },
		[HID_KEY_] = /* A6      F6          */ { 0x0A, 0x8A, 0xC5,     0x05,   0xF5,   0xC5,     0xE5,    No  },
		[HID_KEY_] = /* A7      F7          */ { 0x0B, 0x8B, 0xC6,     0x06,   0xF6,   0xC6,     0xE6,    No  },
		[HID_KEY_] = /* A8      F8          */ { 0x0C, 0x8C, 0xC7,     0x07,   0xF7,   0xC7,     0xE7,    No  },
		[HID_KEY_] = /* A9      F9          */ { 0x0D, 0x8D, 0x1F,     0x2F,   0x3F,   0x1F,     0xBD,    No  },
#else
		[HID_KEY_] = /* E13     POP         */ { 0x6C, 0xEC, 0x80,     0x90,   0x80,   0x80,     0xA0,    No  },
		[HID_KEY_] = /* LDO     [<-]        */ { 0x40, 0xC0, 0x87,     0x97,   0x87,   0x87,     0xA7,    No  },
		[HID_KEY_] = /* LD2     [->]        */ { 0x42, 0xC2, 0x89,     0x99,   0x89,   0x89,     0xA9,    No  },
		[HID_KEY_] = /* RD4     Numpad -    */ { 0x58, 0xD8, 0xFE2D,   0xFE5F, NOP,    0xFE2D,   NOP,     No  },
		[HID_KEY_] = /* LC0   7 Home        */ { 0x27, 0xA7, 0x84,     0x94,   0x84,   0x84,     0xA4,    No  },
		[HID_KEY_] = /* LD1   8 Cursor Up   */ { 0x41, 0xC1, 0x88,     0x98,   0x88,   0x88,     0xA8,    Yes },
		[HID_KEY_] = /* LF0   9 Roll Up     */ { 0x72, 0xF2, 0x8D,     0x9D,   0x8D,   0x8D,     0xAD,    No  },
		[HID_KEY_] = /* RC4     Numpad +    */ { 0x3F, 0xBF, 0xFE2B,   0xFE3D, NOP,    0xFE2B,   NOP,     No  },
		[HID_KEY_] = /* LE0   4 Cursor left */ { 0x59, 0xD9, 0x8A,     0x9A,   0x9A,   0x9A,     0xAA,    Yes },
		[HID_KEY_] = /* LE1     NEXT WINDOW */ { 0x5A, 0xDA, 0x8B,     0x9B,   0x8B,   0x8B,     0xAB,    No  },
		[HID_KEY_] = /* LE2   6 Cursor right*/ { 0x5B, 0xDB, 0x8C,     0x9C,   0x8C,   0xBE,     0xAC,    Yes },
		[HID_KEY_] = /*         Numpad =    */ { 0x7C, 0xFC, 0xFEC8,   0xFED8, NOP,    0xFECB,   NOP,     No  },
		[HID_KEY_] = /* LC2   1 End         */ { 0x29, 0xA9, 0x86,     0x96,   0x86,   0x86,     0xA6,    No  },
		[HID_KEY_] = /* LF1   2 Cursor down */ { 0x73, 0xF3, 0x8E,     0x9E,   0x8E,   0x8E,     0xAE,    Yes },
		[HID_KEY_] = /* LF2   3 Roll Down   */ { 0x74, 0xF4, 0x8F,     0x9F,   0x8F,   0x8F,     0xAF,    No  },
		[HID_KEY_] = /* RF3     ENTER       */ { 0x7C, 0xFC, 0xFECB,   0xFEDB, NOP,    0xFECB,   NOP,     No  },
		[HID_KEY_] = /* LE1     NEXT WINDOW */ { 0x5A, 0xDA, 0x8B,     0x9B,   0x8B,   0x8B,     0xAB,    No  },
		[HID_KEY_] = /*         Numpad ,    */ { NOP,  NOP,  NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /* E13     POP         */ { 0x6C, 0xEC, 0x80,     0x90,   0x80,   0x80,     0xA0,    No  },

		[HID_KEY_] = /* LC1  F1/SHELL/CMD   */ { 0x28, 0xA8, 0x85,     0x95,   0x85,   0x85,     0xA5,    No  },
		[HID_KEY_] = /* LB0  F2/CUT/COPY    */ { 0x13, 0x93, 0xB0,     0xB4,   0xB0,   0xB0,     0xB8,    No  },
		[HID_KEY_] = /* LB1  F3/UNDO/PASTE  */ { 0x14, 0x94, 0xB1,     0xB5,   0xB1,   0xB1,     0xB9,    No  },
		[HID_KEY_] = /* LB2  F4/MOVE/GROW   */ { 0x15, 0x95, 0xB2,     0xB6,   0xB2,   0xB2,     0xBA,    No  },

		[HID_KEY_] = /* LAO  F5/INS/MARK    */ { 0x01, 0x81, 0x81,     0x91,   0x81,   0x81,     0xA1,    No  },
		[HID_KEY_] = /* LA1  F6/LINE DEL    */ { 0x02, 0x82, 0x82,     0x92,   0x82,   0x82,     0xA2,    No  },
		[HID_KEY_] = /* LA2  F7/CHAR DEL    */ { 0x03, 0x83, 0x83,     0x93,   0x83,   0x83,     0xA3,    Yes },
		[HID_KEY_] = /* RA0  F8/AGAIN       */ { 0x0E, 0x8E, 0xCD,     0xE9,   0xCD,   0xCD,     0xED,    No  },

		[HID_KEY_] = /* RA1  F9/READ        */ { 0x0F, 0x8F, 0xCE,     0xEA,   0xCE,   0xCE,     0xEE,    No  },
		[HID_KEY_] = /* RA2 F10/SAVE/EDIT   */ { 0x10, 0x90, 0xCF,     0xEB,   0xCF,   0xCF,     0xEF,    No  },
#endif
		[HID_KEY_] = /* RA3 F11/ABORT/EXIT  */ { 0x11, 0x91, 0xDD,     0xEC,   0xD0,   0xD0,     0xFD,    No  },
		[HID_KEY_] = /* RA4 F12/HELP/HOLD   */ { 0x12, 0x92, 0xB3,     0xB7,   0xB3,   0xB3,     0xBB,    No  },

		[HID_KEY_] = /* LE1     NEXT WINDOW */ { 0x5A, 0xDA, 0x8B,     0x9B,   0x8B,   0x8B,     0xAB,    No  },
		[HID_KEY_] = /* LE1     NEXT WINDOW */ { 0x5A, 0xDA, 0x8B,     0x9B,   0x8B,   0x8B,     0xAB,    No  },

// not yet used:
		[HID_KEY_] = /* E0      REPEAT      */ { 0x5D, 0xDD, NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
		[HID_KEY_] = /*        CAPS LOCK LED*/ { 0x7E, 0xFE, NOP,      NOP,    NOP,    NOP,      NOP,     NOP },
#if false
// german kbd:
		[HID_KEY_] = /* 0x68 B1_DE   _      */ { 0x17, 0x97, 0x60,     0x7E,   0x1E,   0x60,     NOP,     No  },
		[HID_KEY_] = /* 0x69 E1a_DE  <>     */ { 0x5F, 0xDF, 0xBE,     0xBE,   NOP,    0xBE,     NOP,     No  },
		[HID_KEY_] = /* 0x6a B14_DE  ESC    */ { 0x16, 0x96, 0x1B,     0x1B,   NOP,    0x1B,     NOP,     No  },
		[HID_KEY_] = /* 0x6b D14_DE  # \    */ { 0x51, 0xD1, 0xC8,     0xC9,   NOP,    0xC8,     NOP,     No  },

		/* 0x6c NPG     NP (   */ { 0x3F, 0xBF, 0xFE28,   0xFE0E, NOP,    0xFE28,   NOP,     No  },
		/* 0x6d NPF     NP )   */ { 0x58, 0xD8, 0xFE29,   0xFE0F, NOP,    0xFE29,   NOP,     No  },
		/* 0x6e NPD     NP +   */ { 0x3B, 0xBB, 0xFE2B,   0xFE26, NOP,    0xFE2B,   NOP,     No  },
		/* 0x6f NPC     NP -   */ { 0x54, 0xD4, 0xFE2D,   0xFE7E, NOP,    0xFE2D,   NOP,     No  },
		/* 0x70 NPB     NP *   */ { 0x6D, 0xED, 0xFE2A,   0xFE21, NOP,    0xFE2A,   NOP,     No  },
		/* 0x71 NPA     NP /   */ { 0x78, 0xF8, 0xFECC,   0xFEC8, NOP,    0xFECC,   NOP,     No  },
		/* 0x72 NPP     NP .   */ { 0x7B, 0xFB, 0xFE2E,   0xFE2C, NOP,    0xFE2E,   NOP,     No  },
		/* 0x73 NPE     ENTER  */ { 0x7C, 0xFC, 0xFECB,   0xFE3D, NOP,    0xFECB,   NOP,     No  },

		/* 0x74 A0      F0     */ { 0x04, 0x84, 0x1C,     0x5C,   0x7C,   0x1C,     0xBC,    No  },
		/* 0x75 A1      F1     */ { 0x05, 0x85, 0xC0,     0xD0,   0xF0,   0xC0,     0xE0,    No  },
		/* 0x76 A2      F2     */ { 0x06, 0x86, 0xC1,     0x01,   0xF1,   0xC1,     0xE1,    No  },
		/* 0x77 A3      F3     */ { 0x07, 0x87, 0xC2,     0x02,   0xF2,   0xC2,     0xE2,    No  },
		/* 0x78 A4      F4     */ { 0x08, 0x88, 0xC3,     0x03,   0xF3,   0xC3,     0xE3,    No  },
		/* 0x79 A5      F5     */ { 0x09, 0x89, 0xC4,     0x04,   0xF4,   0xC4,     0xE4,    No  },
		/* 0x7a A6      F6     */ { 0x0A, 0x8A, 0xC5,     0x05,   0xF5,   0xC5,     0xE5,    No  },
		/* 0x7b A7      F7     */ { 0x0B, 0x8B, 0xC6,     0x06,   0xF6,   0xC6,     0xE6,    No  },
		/* 0x7c A8      F8     */ { 0x0C, 0x8C, 0xC7,     0x07,   0xF7,   0xC7,     0xE7,    No  },
		/* 0x7d A9      F9     */ { 0x0D, 0x8D, 0x1F,     0x2F,   0x3F,   0x1F,     0xBD,    No  },
#endif
#endif
		/* Key   | Keycap      | Down | Up  |Unshifted|Shifted|Control|Caps Lock|Up Trans|Auto  */
		/* Number| Legend      | Code | Code|Code     | Code  | Code  |Code     | Code   |Repeat*/
};
