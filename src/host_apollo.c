#include <bsp/board.h>
#include <tusb.h>

#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/irq.h>

#include "host.h"
#include "hid_codes.h"

#define DEBUG_TAG "apollo"
#include "debug.h"

#define UART_KBD_ID uart1
#define UART_KBD_IRQ UART1_IRQ
#define UART_KBD_TX_PIN 4
#define UART_KBD_RX_PIN 5

typedef enum {
    Mode0_Compatibility = 0,
    Mode1_Keystate = 1,
    Mode2_RelativeCursorControl = 2,
    Mode3_AbsoluteCursorControl = 3
} KeyboardMode;

typedef enum {
    State_Down = 0,
    State_Up,
    State_Unshifted,
    State_Shifted,
    State_Control,
    StateMax
} KeyState;

static KeyboardMode kbd_mode = Mode0_Compatibility;

static void on_keyboard_rx();

// defined at end of file
// [2] = 0 or gui
// [256] = hid code
// [State] = KeyState
static uint16_t s_code_table[2][256][StateMax];

static void kbd_xmit(char c) {
	uart_putc_raw(UART_KBD_ID, c);
}

static void force_mode(KeyboardMode mode) {
	DBG("Setting keyboard mode to %d\n", mode);
	kbd_xmit(0xff);
	kbd_xmit((char) mode);
	kbd_mode = mode;
}

static void set_mode(KeyboardMode mode) {
	if (kbd_mode != mode) {
		force_mode(mode);
	}
}

void apollo_init() {
  gpio_set_function(UART_KBD_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_KBD_RX_PIN, GPIO_FUNC_UART);

  uart_init(UART_KBD_ID, 1200);
  uart_set_hw_flow(UART_KBD_ID, false, false);
  uart_set_format(UART_KBD_ID, 8, 1, UART_PARITY_EVEN);

  irq_set_exclusive_handler(UART_KBD_IRQ, on_keyboard_rx);
  irq_set_enabled(UART_KBD_IRQ, true);

  uart_set_irq_enables(UART_KBD_ID, true, false);
}

void apollo_update() {
}

void apollo_kbd_event(KeyboardEvent* events, uint16_t count) {
	// current state of things, for kbd mode 0
	static bool ctrl = false;
	static bool shift = false;
	static bool alt = false;

	// the windows key or right-alt, which we'll use to trigger
	// a number of the extra Apollo keys
	static bool gui = false;

	if (kbd_mode == Mode0_Compatibility) {
		for (uint16_t i = 0; i < count; ++i) {
			KeyboardEvent event = events[i];

			switch (event.keycode) {
				case HID_KEY_LEFT_CONTROL:
				case HID_KEY_RIGHT_CONTROL:
					ctrl = event.down;
					break;
				case HID_KEY_LEFT_SHIFT:
				case HID_KEY_RIGHT_SHIFT:
					shift = event.down;
					break;
				case HID_KEY_LEFT_ALT:
					alt = event.down;
					break;
				case HID_KEY_RIGHT_ALT:
				case HID_KEY_LEFT_GUI:
				case HID_KEY_RIGHT_GUI:
					gui = event.down;
					continue; // don't send to the host.
				default:
					break;
			}

			uint16_t code;

			if (ctrl)
				code = s_code_table[gui][event.keycode][State_Control];
			else if (shift)
				code = s_code_table[gui][event.keycode][State_Shifted];
			else
				code = s_code_table[gui][event.keycode][State_Unshifted];
			
			//DBG("Mode0: Translating %02x to %04x (%s %s)\n", hidcode, code, ctrl ? "ctrl" : "", shift ? "shift" : "");
			if (code != 0) {
				kbd_xmit(code);
			}
		}

		return;
	}

	for (uint16_t i = 0; i < count; ++i) {
		KeyboardEvent event = events[i];
		if (event.keycode == HID_KEY_RIGHT_ALT || event.keycode == HID_KEY_LEFT_GUI || event.keycode == HID_KEY_RIGHT_GUI) {
			gui = event.down;
			continue;
		}

		uint16_t code = s_code_table[gui][event.keycode][event.down ? State_Down : State_Up];
		kbd_xmit(code);
	}
}

void apollo_mouse_event(const MouseEvent* events, uint8_t count) {
	if (kbd_mode == Mode0_Compatibility) {
		// don't report mouse status in mode 0
		// there's something about sending the same report prefixed with a 0xdf in
		// the MAME code, but I think that would just cause garbage in MD?
		return;
	}

	set_mode(Mode2_RelativeCursorControl);

	for (uint16_t i = 0; i < count; ++i) {
		kbd_xmit(0xf0 ^ events[i].buttons_current);
		kbd_xmit(events[i].dx);
		kbd_xmit(events[i].dy);
	}
}

static void kbd_tx_str(const char *str) {
	while (*str) {
		kbd_xmit(*str++);
	}
}

void on_keyboard_rx() {
    static uint32_t kbd_cmd = 0;
    static bool kbd_reading_cmd = false;
    static int kbd_cmd_bytes = 0;
	static bool first_irq = true;

    while (uart_is_readable(UART_KBD_ID)) {
        uint8_t ch = uart_getc(UART_KBD_ID);

        if (!kbd_reading_cmd) {
			if (ch == 0x00) {
				// ignore
			} else if (ch == 0xff) {
                kbd_reading_cmd = true;
                kbd_cmd = 0;
                kbd_cmd_bytes = 0;
            } else {
                DBG("Unknown command start byte: %02x\n", ch);
				continue;
            }
        } else {
			if (kbd_cmd_bytes == 4) {
				DBG("Too-long keyboard command: currently %08lx, got %02x\n", kbd_cmd, ch);
				kbd_reading_cmd = false;
				continue;
			}
			kbd_cmd = (kbd_cmd << 8) | ch;
			kbd_cmd_bytes++;
        }

        DBG(" command %08lx (%d bytes)\n", kbd_cmd, kbd_cmd_bytes);

        bool cmd_handled = false;

		if (kbd_cmd_bytes == 1) {
			switch (kbd_cmd) {
				case 0x00:
					force_mode(Mode0_Compatibility);
					cmd_handled = true;
					break;
				
				case 0x01:
					force_mode(Mode1_Keystate);
					cmd_handled = true;
					break;
			}
		} else if (kbd_cmd_bytes == 2) {
			switch (kbd_cmd) {
				case 0x1221: // keyboard identification
					DBG("keyboard ident request\n");

					kbd_tx_str("\xff\x12\x21");
					kbd_tx_str("3-@\r2-0\rSD-03863-MS\r"); // english ident
					//kbd_tx_str("3-A\r2-0\rSD-03863-MS\r"); // german ident

					if (kbd_mode == Mode0_Compatibility) {
						force_mode(Mode0_Compatibility);
					} else {
						force_mode(Mode1_Keystate);
					}

					cmd_handled = true;
					break;

				case 0x1116: // unclear? mame sends back two mode0 forces
					force_mode(Mode0_Compatibility);
					force_mode(Mode0_Compatibility);
					cmd_handled = true;
					break;

				case 0x2181: // beeper on for 300ms
				case 0x2182: // beeper off
					cmd_handled = true;
					break;

				case 0x1166: // unknown
				case 0x1117: // unknown
					cmd_handled = true;
					break;
			}
		} else if (kbd_cmd_bytes == 3) {
			switch (kbd_cmd) {
				case 0x10045e: // copilot thinks this is "mouse enable"?
					cmd_handled = true;
					break;
			}
		}

		// after mouse, sometimes the keyboard sends 0xff10045e 00000000

		if (cmd_handled) {
			DBG(" command handled\n");
			kbd_reading_cmd = false;
			kbd_cmd = 0;
			kbd_cmd_bytes = 0;
		}
    }
}

#define Yes 1
#define No 0
#define NONE 0

static uint16_t s_code_table[2][256][StateMax] = {
/******************* Un-modified; no Apollo keys *********************/
{
		                                /*  Down |Up   |Unsh |Shft |Ctrl */
		                                /*  Code |Code |Code |Code |Code */

		/* A0 .. A9 */
		[HID_KEY_F10]                   = { 0x04, 0x84, 0x1C, 0x5C, 0x7C },
		[HID_KEY_F1]                    = { 0x05, 0x85, 0xC0, 0xD0, 0xF0 },
		[HID_KEY_F2]                    = { 0x06, 0x86, 0xC1, 0x01, 0xF1 },
		[HID_KEY_F3]                    = { 0x07, 0x87, 0xC2, 0x02, 0xF2 },
		[HID_KEY_F4]                    = { 0x08, 0x88, 0xC3, 0x03, 0xF3 },
		[HID_KEY_F5]                    = { 0x09, 0x89, 0xC4, 0x04, 0xF4 },
		[HID_KEY_F6]                    = { 0x0A, 0x8A, 0xC5, 0x05, 0xF5 },
		[HID_KEY_F7]                    = { 0x0B, 0x8B, 0xC6, 0x06, 0xF6 },
		[HID_KEY_F8]                    = { 0x0C, 0x8C, 0xC7, 0x07, 0xF7 },
		[HID_KEY_F9]                    = { 0x0D, 0x8D, 0x1F, 0x2F, 0x3F },

		/* B1 .. B15 */
		[HID_KEY_ESCAPE]                = { 0x17, 0x97, 0x1B, 0x1B, NONE },
		[HID_KEY_1]                     = { 0x18, 0x98, 0x31, 0x21, NONE },
		[HID_KEY_2]                     = { 0x19, 0x99, 0x32, 0x40, NONE },
		[HID_KEY_3]                     = { 0x1A, 0x9A, 0x33, 0x23, NONE },
		[HID_KEY_4]                     = { 0x1B, 0x9B, 0x34, 0x24, NONE },
		[HID_KEY_5]                     = { 0x1C, 0x9C, 0x35, 0x25, NONE },
		[HID_KEY_6]                     = { 0x1D, 0x9D, 0x36, 0x5E, NONE },
		[HID_KEY_7]                     = { 0x1E, 0x9E, 0x37, 0x26, NONE },
		[HID_KEY_8]                     = { 0x1F, 0x9F, 0x38, 0x2A, NONE },
		[HID_KEY_9]                     = { 0x20, 0xA0, 0x39, 0x28, NONE },
		[HID_KEY_0]                     = { 0x21, 0xA1, 0x30, 0x29, NONE },
		[HID_KEY_MINUS]                 = { 0x22, 0xA2, 0x2D, 0x5F, NONE },
		[HID_KEY_EQUAL]                 = { 0x23, 0xA3, 0x3D, 0x2B, NONE },
		[HID_KEY_GRAVE]                 = { 0x24, 0xA4, 0x60, 0x7E, 0x1E },
		[HID_KEY_BACKSPACE]             = { 0x25, 0xA5, 0x5E, 0x5E, NONE },

		/* C1 .. C14 */
		[HID_KEY_TAB]                   = { 0x2C, 0xAC, 0xCA, 0xDA, 0xFA },
		[HID_KEY_Q]                     = { 0x2D, 0xAD, 0x71, 0x51, 0x11 },
		[HID_KEY_W]                     = { 0x2E, 0xAE, 0x77, 0x57, 0x17 },
		[HID_KEY_E]                     = { 0x2F, 0xAF, 0x65, 0x45, 0x05 },
		[HID_KEY_R]                     = { 0x30, 0xB0, 0x72, 0x52, 0x12 },
		[HID_KEY_T]                     = { 0x31, 0xB1, 0x74, 0x54, 0x14 },
		[HID_KEY_Y]                     = { 0x32, 0xB2, 0x59, 0x59, 0x19 },
		[HID_KEY_U]                     = { 0x33, 0xB3, 0x75, 0x55, 0x15 },
		[HID_KEY_I]                     = { 0x34, 0xB4, 0x69, 0x49, 0x09 },
		[HID_KEY_O]                     = { 0x35, 0xB5, 0x6F, 0x4F, 0x0F },
		[HID_KEY_P]                     = { 0x36, 0xB6, 0x70, 0x50, 0x10 },
		[HID_KEY_BRACKET_LEFT]          = { 0x37, 0xB7, 0x7B, 0x5B, 0x1B },
		[HID_KEY_BRACKET_RIGHT]         = { 0x38, 0xB8, 0x7D, 0x5D, 0x1D },
		[HID_KEY_DELETE]                = { 0x3A, 0xBA, 0x7F, 0x7F, NONE },

		/* D0 .. D14 */
		[HID_KEY_LEFT_CONTROL]          = { 0x43, 0xC3, NONE, NONE, NONE },
		[HID_KEY_CAPS_LOCK]             = { NONE, NONE, NONE, NONE, NONE },
		[HID_KEY_A]                     = { 0x46, 0xC6, 0x61, 0x41, 0x01 },
		[HID_KEY_S]                     = { 0x47, 0xC7, 0x73, 0x53, 0x13 },
		[HID_KEY_D]                     = { 0x48, 0xC8, 0x64, 0x44, 0x04 },
		[HID_KEY_F]                     = { 0x49, 0xC9, 0x66, 0x46, 0x06 },
		[HID_KEY_G]                     = { 0x4A, 0xCA, 0x67, 0x47, 0x07 },
		[HID_KEY_H]                     = { 0x4B, 0xCB, 0x68, 0x48, 0x08 },
		[HID_KEY_J]                     = { 0x4C, 0xCC, 0x6A, 0x4A, 0x0A },
		[HID_KEY_K]                     = { 0x4D, 0xCD, 0x6B, 0x4B, 0x0B },
		[HID_KEY_L]                     = { 0x4E, 0xCE, 0x6C, 0x4C, 0x0C },
		[HID_KEY_SEMICOLON]             = { 0x4F, 0xCF, 0x3B, 0x3A, 0xFB },
		[HID_KEY_APOSTROPHE]            = { 0x50, 0xD0, 0x27, 0x22, 0xF8 },
		[HID_KEY_ENTER]                 = { 0x52, 0xD2, 0x0D, NONE, NONE },
		[HID_KEY_BACKSLASH]             = { 0x53, 0xD3, 0xC8, 0xC9, NONE },

		/* E0 .. E13 */
		/* E0 = REPT - Apollo specific */
		[HID_KEY_LEFT_SHIFT]            = { 0x5E, 0xDE, NONE, NONE, NONE },
		[HID_KEY_Z]                     = { 0x60, 0xE0, 0x5A, 0x5A, 0x1A },
		[HID_KEY_X]                     = { 0x61, 0xE1, 0x78, 0x58, 0x18 },
		[HID_KEY_C]                     = { 0x62, 0xE2, 0x63, 0x43, 0x03 },
		[HID_KEY_V]                     = { 0x63, 0xE3, 0x76, 0x56, 0x16 },
		[HID_KEY_B]                     = { 0x64, 0xE4, 0x62, 0x42, 0x02 },
		[HID_KEY_N]                     = { 0x65, 0xE5, 0x6E, 0x4E, 0x0E },
		[HID_KEY_M]                     = { 0x66, 0xE6, 0x6D, 0x4D, 0x0D },
		[HID_KEY_COMMA]                 = { 0x67, 0xE7, 0x2C, 0x3C, NONE },
		[HID_KEY_PERIOD]                = { 0x68, 0xE8, 0x2E, 0x3E, NONE },
		[HID_KEY_SLASH]                 = { 0x69, 0xE9, 0xCC, 0xDC, 0xFC },
		[HID_KEY_RIGHT_SHIFT]           = { 0x6A, 0xEA, NONE, NONE, NONE },
		/* E13 = POP - Apollo specific */


		/* ?? not on the map */
		[HID_KEY_LEFT_ALT]              = { 0x75, 0xF5, NONE, NONE, NONE },
		/* F1 */
		[HID_KEY_SPACE]                 = { 0x76, 0xF6, 0x20, 0x20, 0x20 },
		/* ?? -- not used, this is an Apollo trigger */
		[HID_KEY_RIGHT_ALT]             = { 0x77, 0xF7, NONE, NONE, NONE },

		[HID_KEY_HOME]                  = { 0x27, 0xA7, 0x84, 0x94, 0x84 },
		[HID_KEY_PAGE_UP]               = { 0x72, 0xF2, 0x8D, 0x9D, 0x8D },
		[HID_KEY_PAGE_DOWN]             = { 0x74, 0xF4, 0x8F, 0x9F, 0x8F },
		[HID_KEY_END]                   = { 0x29, 0xA9, 0x86, 0x96, 0x86 },
		[HID_KEY_ARROW_LEFT]            = { 0x59, 0xD9, 0x8A, 0x9A, 0x9A },
		[HID_KEY_ARROW_UP]              = { 0x41, 0xC1, 0x88, 0x98, 0x88 },
		[HID_KEY_ARROW_RIGHT]           = { 0x5B, 0xDB, 0x8C, 0x9C, 0x8C },
		[HID_KEY_ARROW_DOWN]            = { 0x73, 0xF3, 0x8E, 0x9E, 0x8E },

		/* Keypad */
		/* RC1..4, RD1..4, RE1..3, RF1..3 */
		[HID_KEY_KEYPAD_7_HOME]         = { 0x3C, 0xBC, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_8_UP_ARROW]     = { 0x3D, 0xBD, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_9_PAGEUP]       = { 0x3E, 0xBE, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_PLUS]           = { 0x3F, 0xBF, NONE, NONE, NONE },

		[HID_KEY_KEYPAD_4_LEFT_ARROW]   = { 0x55, 0xD5, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_5]              = { 0x56, 0xD6, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_6_RIGHT_ARROW]  = { 0x57, 0xD7, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_MINUS]          = { 0x58, 0xD8, NONE, NONE, NONE },

		[HID_KEY_KEYPAD_1_END]          = { 0x6E, 0xEE, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_2_DOWN_ARROW]   = { 0x6F, 0xEF, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_3_PAGEDN]       = { 0x70, 0xF0, NONE, NONE, NONE },

		[HID_KEY_KEYPAD_0_INSERT]       = { 0x79, 0xF9, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_DECIMAL]        = { 0x7B, 0xFB, NONE, NONE, NONE },
		[HID_KEY_KEYPAD_ENTER]          = { 0x7C, 0xFC, NONE, NONE, NONE },
},

/******************* Modified; Apollo keys *********************/
{
		                          /* MAME? | Keycap         | Down | Up  |Unshifted|Shifted|Control|Caps Lock|Up Trans|Auto  */
		                          /* Number| Legend         | Code | Code|Code     | Code  | Code  |Code     | Code   |Repeat*/

#if false
		[HID_KEY_DELETE] = /*    POP         */ { 0x6C, 0xEC, 0x80,     0x90,   0x80,   0x80,     0xA0,    No  },

/*		[HID_KEY_KEYPAD_NUM_LOCK_AND_CLEAR] = /*         Numpad CLR  */ { NONE,  NONE,  NONE,      NONE,    NONE,    NONE,      NONE,     NONE },
		[HID_KEY_] = /*    POP         */ { 0x6C, 0xEC, 0x80,     0x90,   0x80,   0x80,     0xA0,    No  },
		[HID_KEY_] = /*   [<-]        */ { 0x40, 0xC0, 0x87,     0x97,   0x87,   0x87,     0xA7,    No  },
		[HID_KEY_] = /*   [->]        */ { 0x42, 0xC2, 0x89,     0x99,   0x89,   0x89,     0xA9,    No  },
		[HID_KEY_] = /*   Numpad -    */ { 0x58, 0xD8, 0xFE2D,   0xFE5F, NONE,    0xFE2D,   NONE,     No  },
		[HID_KEY_] = /* 7 Home        */ { 0x27, 0xA7, 0x84,     0x94,   0x84,   0x84,     0xA4,    No  },
		[HID_KEY_] = /* 8 Cursor Up   */ { 0x41, 0xC1, 0x88,     0x98,   0x88,   0x88,     0xA8,    Yes },
		[HID_KEY_] = /* 9 Roll Up     */ { 0x72, 0xF2, 0x8D,     0x9D,   0x8D,   0x8D,     0xAD,    No  },
		[HID_KEY_] = /*   Numpad +    */ { 0x3F, 0xBF, 0xFE2B,   0xFE3D, NONE,    0xFE2B,   NONE,     No  },
		[HID_KEY_] = /* 4 Cursor left */ { 0x59, 0xD9, 0x8A,     0x9A,   0x9A,   0x9A,     0xAA,    Yes },
		[HID_KEY_] = /*   NEXT WINDOW */ { 0x5A, 0xDA, 0x8B,     0x9B,   0x8B,   0x8B,     0xAB,    No  },
		[HID_KEY_] = /* 6 Cursor right*/ { 0x5B, 0xDB, 0x8C,     0x9C,   0x8C,   0xBE,     0xAC,    Yes },
		[HID_KEY_] = /*         Numpad =    */ { 0x7C, 0xFC, 0xFEC8,   0xFED8, NONE,    0xFECB,   NONE,     No  },
		[HID_KEY_] = /* 1 End         */ { 0x29, 0xA9, 0x86,     0x96,   0x86,   0x86,     0xA6,    No  },
		[HID_KEY_] = /* 2 Cursor down */ { 0x73, 0xF3, 0x8E,     0x9E,   0x8E,   0x8E,     0xAE,    Yes },
		[HID_KEY_] = /* 3 Roll Down   */ { 0x74, 0xF4, 0x8F,     0x9F,   0x8F,   0x8F,     0xAF,    No  },
		[HID_KEY_] = /*   ENTER       */ { 0x7C, 0xFC, 0xFECB,   0xFEDB, NONE,    0xFECB,   NONE,     No  },
		[HID_KEY_] = /*   NEXT WINDOW */ { 0x5A, 0xDA, 0x8B,     0x9B,   0x8B,   0x8B,     0xAB,    No  },
		[HID_KEY_] = /*         Numpad ,    */ { NONE,  NONE,  NONE,      NONE,    NONE,    NONE,      NONE,     NONE },
		[HID_KEY_] = /*    POP         */ { 0x6C, 0xEC, 0x80,     0x90,   0x80,   0x80,     0xA0,    No  },

		[HID_KEY_] = /*F1/SHELL/CMD   */ { 0x28, 0xA8, 0x85,     0x95,   0x85,   0x85,     0xA5,    No  },
		[HID_KEY_] = /*F2/CUT/COPY    */ { 0x13, 0x93, 0xB0,     0xB4,   0xB0,   0xB0,     0xB8,    No  },
		[HID_KEY_] = /*F3/UNDO/PASTE  */ { 0x14, 0x94, 0xB1,     0xB5,   0xB1,   0xB1,     0xB9,    No  },
		[HID_KEY_] = /*F4/MOVE/GROW   */ { 0x15, 0x95, 0xB2,     0xB6,   0xB2,   0xB2,     0xBA,    No  },

		[HID_KEY_] = /*F5/INS/MARK    */ { 0x01, 0x81, 0x81,     0x91,   0x81,   0x81,     0xA1,    No  },
		[HID_KEY_] = /*F6/LINE DEL    */ { 0x02, 0x82, 0x82,     0x92,   0x82,   0x82,     0xA2,    No  },
		[HID_KEY_] = /*F7/CHAR DEL    */ { 0x03, 0x83, 0x83,     0x93,   0x83,   0x83,     0xA3,    Yes },
		[HID_KEY_] = /*F8/AGAIN       */ { 0x0E, 0x8E, 0xCD,     0xE9,   0xCD,   0xCD,     0xED,    No  },

		[HID_KEY_] = /*F9/READ        */ { 0x0F, 0x8F, 0xCE,     0xEA,   0xCE,   0xCE,     0xEE,    No  },
		[HID_KEY_] = /*F10/SAVE/EDIT   */ { 0x10, 0x90, 0xCF,     0xEB,   0xCF,   0xCF,     0xEF,    No  },
		[HID_KEY_] = /*F11/ABORT/EXIT  */ { 0x11, 0x91, 0xDD,     0xEC,   0xD0,   0xD0,     0xFD,    No  },
		[HID_KEY_] = /*F12/HELP/HOLD   */ { 0x12, 0x92, 0xB3,     0xB7,   0xB3,   0xB3,     0xBB,    No  },

		[HID_KEY_] = /*   NEXT WINDOW */ { 0x5A, 0xDA, 0x8B,     0x9B,   0x8B,   0x8B,     0xAB,    No  },
		[HID_KEY_] = /*   NEXT WINDOW */ { 0x5A, 0xDA, 0x8B,     0x9B,   0x8B,   0x8B,     0xAB,    No  },

// not yet used:
		[HID_KEY_] = /*    REPEAT      */ { 0x5D, 0xDD, NONE,      NONE,    NONE,    NONE,      NONE,     NONE },
		[HID_KEY_] = /*        CAPS LOCK LED*/ { 0x7E, 0xFE, NONE,      NONE,    NONE,    NONE,      NONE,     NONE },
#endif
		/* | Keycap      | Down | Up  |Unshifted|Shifted|Control|Caps Lock|Up Trans|Auto  */
}
};
