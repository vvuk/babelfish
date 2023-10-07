#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <stdint.h>
#include <tusb.h>

typedef struct {
    // HID usage page
    uint16_t page;

    // HID keycode within that page
    uint16_t keycode;

    uint16_t down : 1; // was key pressed this frame
    uint16_t reserved : 15;
} KeyboardEvent;

typedef struct {
    // relative mouse motion
    int8_t dx;
    int8_t dy;

    // relative wheel motion
    int8_t dwheel;

    // buttons just pressed
    uint8_t buttons_down;

    // buttons just released
    uint8_t buttons_up;

    // current buttons
    uint8_t buttons;
} MouseEvent;

#define EVENT_IS_HOST_MOD(event) (event.keycode == HID_KEY_LEFT_GUI || event.keycode == HID_KEY_RIGHT_GUI || event.keycode == HID_KEY_RIGHT_ALT)

#define MAX_QUEUED_EVENTS 32

void enqueue_kbd_event(const KeyboardEvent* event);
void enqueue_mouse_event(const MouseEvent* event);
void get_queued_kbd_events(KeyboardEvent* events, uint* count);
void get_queued_mouse_events(MouseEvent* events, uint* count);

void babelfish_uart_config(int uidx, char ab);

void translate_boot_kbd_report(hid_keyboard_report_t const *report);
void translate_boot_mouse_report(hid_mouse_report_t const *report);

#endif