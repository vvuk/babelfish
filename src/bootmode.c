#include "babelfish.h"
#include "host.h"
#include "hid_codes.h"

void
translate_boot_kbd_report(hid_keyboard_report_t const *report, HostDevice* host)
{
	static uint8_t mod_down_state = 0;

    uint8_t up_keys[12]; // up to 12 keys could have been released this frame (6 + modifiers)
    uint8_t up_key_count = 0;
	uint8_t new_mod_down = 0;

	// This is messy. We really want raw key down/up from HID, but we're not in that mode.
    // So we have to reconstruct. Rule is -- modifiers go down before keys; keys go up before modifiers.

#if false
    static bool down_state[256] = {false};
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
#else
	// find any released keys
	static uint8_t down_keys[6] = { 0 };

	for (int i = 0; i < 6; i++) {
		uint8_t prev_down_key = down_keys[i];
		if (prev_down_key == 0)
			continue;

		bool released = true;
		for (int j = 0; j < 6; j++) {
			if (report->keycode[j] == prev_down_key) {
				released = false;
				break;
			}
		}

		if (released) {
			up_keys[up_key_count++] = down_keys[i];
			down_keys[i] = 0;
		}
	}

	memcpy(down_keys, report->keycode, 6);
#endif

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

    uint32_t written_events = 0;
#define WRITE_EVENT(page, code, downval) \
	do { \
		KeyboardEvent evt = {page, code, .down = downval}; \
		static_kbd_events[written_events++] = evt; \
	} while (0)

	// write all the released keys
	for (int i = 0; i < up_key_count; i++) {
		uint8_t hidcode = up_keys[i];
		if (hidcode == 0)
			continue;
		WRITE_EVENT(0, hidcode, false);
	}

	// now new down modifiers
	if (new_mod_down) {
		if (new_mod_down & KEYBOARD_MODIFIER_LEFTSHIFT) {
			WRITE_EVENT(0, HID_KEY_LEFT_SHIFT, false);
		}
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTSHIFT) {
			WRITE_EVENT(0, HID_KEY_RIGHT_SHIFT, false);
		}
        if (new_mod_down & KEYBOARD_MODIFIER_LEFTCTRL) {
			WRITE_EVENT(0, HID_KEY_LEFT_CONTROL, false);
		}
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTCTRL) {
			WRITE_EVENT(0, HID_KEY_RIGHT_CONTROL, false);
		}
        if (new_mod_down & KEYBOARD_MODIFIER_LEFTALT) {
			WRITE_EVENT(0, HID_KEY_LEFT_ALT, false);
		}
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTALT) {
			WRITE_EVENT(0, HID_KEY_RIGHT_ALT, false);
		}
	}

	for (int i = 0; i < 6; i++) {
		uint8_t hidcode = report->keycode[i];
		if (hidcode == 0)
			continue;

		WRITE_EVENT(0, hidcode, true);
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
