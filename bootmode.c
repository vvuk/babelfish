#include "babelfish.h"
#include "host.h"

void
translate_boot_kbd_report(hid_keyboard_report_t const *report, HostDevice* host)
{
    static bool down_state[256] = {false};
	static uint8_t mod_down_state = 0;

    uint8_t up_keys[12]; // up to 12 keys could have been released this frame (6 + modifiers)
    uint8_t up_key_count = 0;
	uint8_t new_mod_down = 0;

	// This is messy.  We really want raw key down/up from HID, but we're not in that mode.  So we have to reconstruct.
	// Rule is -- modifiers go down before keys; keys go up before modifiers.
	if (mod_down_state ^ report->modifier) {
		uint8_t up_mod = mod_down_state ^ report->modifier;
		new_mod_down = report->modifier & ~mod_down_state;
		mod_down_state = report->modifier;

		if (up_mod & KEYBOARD_MODIFIER_LEFTSHIFT) {
			up_keys[up_key_count++] = HID_KEY_SHIFT_LEFT;
		} else if (up_mod & KEYBOARD_MODIFIER_RIGHTSHIFT) {
			up_keys[up_key_count++] = HID_KEY_SHIFT_RIGHT;
		} else if (up_mod & KEYBOARD_MODIFIER_LEFTCTRL) {
			up_keys[up_key_count++] = HID_KEY_CONTROL_LEFT;
		} else if (up_mod & KEYBOARD_MODIFIER_RIGHTCTRL) {
			up_keys[up_key_count++] = HID_KEY_CONTROL_RIGHT;
		} else if (up_mod & KEYBOARD_MODIFIER_LEFTALT) {
			up_keys[up_key_count++] = HID_KEY_ALT_LEFT;
		} else if (up_mod & KEYBOARD_MODIFIER_RIGHTALT) {
			up_keys[up_key_count++] = HID_KEY_ALT_RIGHT;
		}
	}

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

    uint32_t written_events = 0;

	// write all the released keys
	for (int i = 0; i < up_key_count; i++) {
		uint8_t hidcode = up_keys[i];
		uint16_t code = s_code_table[hidcode][State_Up];
		if (code != 0) {
            static_kbd_events[written_events++] = {0, code, .down = false};
		}
	}

	// now new down modifiers
	if (new_mod_down) {
		if (new_mod_down & KEYBOARD_MODIFIER_LEFTSHIFT) {
            static_kbd_events[written_events++] = {0, code, .down = false};
			uint16_t code = s_code_table[HID_KEY_SHIFT_LEFT][State_Down];
			kbd_xmit(code);
		} else if (new_mod_down & KEYBOARD_MODIFIER_RIGHTSHIFT) {
			uint16_t code = s_code_table[HID_KEY_SHIFT_RIGHT][State_Down];
			kbd_xmit(code);
		} else if (new_mod_down & KEYBOARD_MODIFIER_LEFTCTRL) {
			uint16_t code = s_code_table[HID_KEY_CONTROL_LEFT][State_Down];
			kbd_xmit(code);
		} else if (new_mod_down & KEYBOARD_MODIFIER_RIGHTCTRL) {
			uint16_t code = s_code_table[HID_KEY_CONTROL_RIGHT][State_Down];
			kbd_xmit(code);
		} else if (new_mod_down & KEYBOARD_MODIFIER_LEFTALT) {
			uint16_t code = s_code_table[HID_KEY_ALT_LEFT][State_Down];
			kbd_xmit(code);
		} else if (new_mod_down & KEYBOARD_MODIFIER_RIGHTALT) {
			uint16_t code = s_code_table[HID_KEY_ALT_RIGHT][State_Down];
			kbd_xmit(code);
		}
	}

	for (int i = 0; i < 6; i++) {
		uint8_t hidcode = report->keycode[i];
		if (hidcode == 0)
			continue;

		uint16_t code = s_code_table[hidcode][State_Down];
		if (code != 0) {
			kbd_xmit(code);
		}
		down_state[hidcode] = true;
	}
}
}
