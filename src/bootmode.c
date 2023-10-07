#define DEBUG_VERBOSE 0
#define DEBUG_TAG "bootmode"
#include "babelfish.h"

#include "hid_codes.h"

#define UP 0
#define DOWN 1

void
translate_boot_kbd_report(hid_keyboard_report_t const *report)
{
	static uint8_t down_keys[6] = { 0 };
	static uint8_t mod_down_state = 0;

	DBG_V("Keyboard: mod: %02x keycodes: %02x %02x %02x %02x %02x %02x\n", report->modifier, report->keycode[0],
		report->keycode[1], report->keycode[2], report->keycode[3], report->keycode[4], report->keycode[5]);
	DBG_V("    known down: %02x %02x %02x %02x %02x %02x\n", down_keys[0], down_keys[1], down_keys[2],
		down_keys[3], down_keys[4], down_keys[5]);

	uint8_t keycodes[6];
    uint8_t up_keys[12]; // up to 12 keys could have been released this frame (6 + modifiers)
    uint8_t up_key_count = 0;
	uint8_t mod_changed = 0;
	uint8_t new_mod_up = 0;
	uint8_t new_mod_down = 0;

	// we may need to modify this
	memcpy(keycodes, report->keycode, 6);

	// This is messy. We really want raw key down/up from HID, but we're not in that mode.
    // So we have to reconstruct. Rule is -- modifiers go down before keys; keys go up before modifiers.

	for (int i = 0; i < 6; i++) {
		uint8_t prev_down_key = down_keys[i];
		if (prev_down_key == 0)
			continue;

		bool released = true;
		for (int j = 0; j < 6; j++) {
			if (keycodes[j] == prev_down_key) {
				// it's still down, don't report it twice
				keycodes[j] = 0;
				released = false;
				break;
			}
		}

		if (released) {
			up_keys[up_key_count++] = down_keys[i];
			down_keys[i] = 0;
		}
	}

	// note! report->keycode, not our keycodes array.
	// we want the original set of down keys, not our mucked one
	memcpy(down_keys, report->keycode, 6);

	mod_changed = mod_down_state ^ report->modifier;
	new_mod_up = mod_changed & ~report->modifier;
	new_mod_down = mod_changed & report->modifier;

	mod_down_state = report->modifier;

    uint32_t written_events = 0;
#define WRITE_EVENT(page, code, downval) \
	do { \
		KeyboardEvent evt = {page, code, .down = downval}; \
		enqueue_kbd_event(&evt); \
	} while (0)

	// write all the released keys
	for (int i = 0; i < up_key_count; i++) {
		uint8_t hidcode = up_keys[i];
		if (hidcode == 0)
			continue;
		WRITE_EVENT(0, hidcode, UP);
	}

	// then the released modifiers
	if (new_mod_up) {
		if (new_mod_up & KEYBOARD_MODIFIER_LEFTSHIFT)
			WRITE_EVENT(0, HID_KEY_LEFT_SHIFT, UP);
		if (new_mod_up & KEYBOARD_MODIFIER_RIGHTSHIFT)
			WRITE_EVENT(0, HID_KEY_RIGHT_SHIFT, UP);
		if (new_mod_up & KEYBOARD_MODIFIER_LEFTCTRL)
			WRITE_EVENT(0, HID_KEY_LEFT_CONTROL, UP);
		if (new_mod_up & KEYBOARD_MODIFIER_RIGHTCTRL)
			WRITE_EVENT(0, HID_KEY_RIGHT_CONTROL, UP);
		if (new_mod_up & KEYBOARD_MODIFIER_LEFTALT)
			WRITE_EVENT(0, HID_KEY_LEFT_ALT, UP);
		if (new_mod_up & KEYBOARD_MODIFIER_RIGHTALT)
			WRITE_EVENT(0, HID_KEY_RIGHT_ALT, UP);
	}

	// now new down modifiers
	if (new_mod_down) {
		if (new_mod_down & KEYBOARD_MODIFIER_LEFTSHIFT)
			WRITE_EVENT(0, HID_KEY_LEFT_SHIFT, DOWN);
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTSHIFT)
			WRITE_EVENT(0, HID_KEY_RIGHT_SHIFT, DOWN);
        if (new_mod_down & KEYBOARD_MODIFIER_LEFTCTRL)
			WRITE_EVENT(0, HID_KEY_LEFT_CONTROL, DOWN);
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTCTRL)
			WRITE_EVENT(0, HID_KEY_RIGHT_CONTROL, DOWN);
        if (new_mod_down & KEYBOARD_MODIFIER_LEFTALT)
			WRITE_EVENT(0, HID_KEY_LEFT_ALT, DOWN);
        if (new_mod_down & KEYBOARD_MODIFIER_RIGHTALT)
			WRITE_EVENT(0, HID_KEY_RIGHT_ALT, DOWN);
	}

	for (int i = 0; i < 6; i++) {
		uint8_t hidcode = keycodes[i];
		if (hidcode == 0)
			continue;
		WRITE_EVENT(0, hidcode, DOWN);
	}
}

void
translate_boot_mouse_report(hid_mouse_report_t const *report)
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
	event.buttons = report->buttons;

    buttons_down = current_buttons_state;

	enqueue_mouse_event(&event);
}
