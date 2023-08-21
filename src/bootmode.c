#include "babelfish.h"
#include "host.h"
#include "hod_codes.h"

void
translate_boot_kbd_report(hid_keyboard_report_t const *report, HostDevice* host)
{
    static bool down_state[256] = {false};
	static uint8_t mod_down_state = 0;

    uint8_t up_keys[12]; // up to 12 keys could have been released this frame (6 + modifiers)
    uint8_t up_key_count = 0;
	uint8_t new_mod_down = 0;

	// This is messy. We really want raw key down/up from HID, but we're not in that mode.
    // So we have to reconstruct. Rule is -- modifiers go down before keys; keys go up before modifiers.
	if (mod_down_state ^ report->modifier) {
		uint8_t up_mod = mod_down_state ^ report->modifier;
		new_mod_down = report->modifier & ~mod_down_state;
		mod_down_state = report->modifier;

		if (up_mod & KEYBOARD_MODIFIER_LEFTSHIFT) {
			up_keys[up_key_count++] = HID_KEY_LEFT_SHIFT;
		} else if (up_mod & KEYBOARD_MODIFIER_RIGHTSHIFT) {
			up_keys[up_key_count++] = HID_KEY_RIGHT_SHIFT;
		} else if (up_mod & KEYBOARD_MODIFIER_LEFTCTRL) {
			up_keys[up_key_count++] = HID_KEY_LEFT_CONTROL;
		} else if (up_mod & KEYBOARD_MODIFIER_RIGHTCTRL) {
			up_keys[up_key_count++] = HID_KEY_RIGHT_CONTROL;
		} else if (up_mod & KEYBOARD_MODIFIER_LEFTALT) {
			up_keys[up_key_count++] = HID_KEY_LEFT_ALT;
		} else if (up_mod & KEYBOARD_MODIFIER_RIGHTALT) {
			up_keys[up_key_count++] = HID_KEY_RIGHT_ALT;
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
            static_kbd_events[written_events++] = {0, HID_KEY_LEFTSHIFT, .down = false};
		}
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTSHIFT) {
            static_kbd_events[written_events++] = {0, HID_KEY_RIGHTSHIFT, .down = false};
		}
        if (new_mod_down & KEYBOARD_MODIFIER_LEFTCTRL) {
            static_kbd_events[written_events++] = {0, HID_KEY_LEFTCTRL, .down = false};
		}
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTCTRL) {
            static_kbd_events[written_events++] = {0, HID_KEY_RIGHTCTRL, .down = false};
		}
        if (new_mod_down & KEYBOARD_MODIFIER_LEFTALT) {
            static_kbd_events[written_events++] = {0, HID_KEY_LEFTALT, .down = false};
		}
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTALT) {
            static_kbd_events[written_events++] = {0, HID_KEY_RIGHTALT, .down = false};
		}
	}

	for (int i = 0; i < 6; i++) {
		uint8_t hidcode = report->keycode[i];
		if (hidcode == 0)
			continue;

        static_kbd_events[written_events++] = {0, hidcode, .down = true};
		down_state[hidcode] = true;
	}

    host->kbd_event(static_kbd_events, written_events);
}

void
translate_boot_mouse_report(hid_mouse_report_t const *report, HostDevice* host)
{
    static uint16_t buttons_down = 0;

    uint16_t current_buttons_state = report->buttons;
    uint16_t changed_buttons = current_buttons_state ^ buttons_down;

    MouseEvent event;
    event.dx = report->x;
    event.dy = report->y;
    event.dwheel = report->wheel;
    event.buttons_down = changed_buttons & current_buttons_state;
    event.buttons_up = changed_buttons & ~current_buttons_state;

    buttons_down = current_buttons_state;

    host->mouse_event(&event, 1);
}
