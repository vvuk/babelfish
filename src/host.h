#ifndef HOST_H
#define HOST_H

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
    uint8_t buttons_current;
} MouseEvent;

typedef struct {
    char name[16];
    void (*init)();
    void (*update)();
    void (*kbd_event)(const KeyboardEvent* events, uint8_t count);
    void (*mouse_event)(const MouseEvent* events, uint8_t count);
} HostDevice;

#define EVENT_IS_HOST_MOD(event) (event.keycode == HID_KEY_LEFT_GUI || event.keycode == HID_KEY_RIGHT_GUI || event.keycode == HID_KEY_RIGHT_ALT)

#define NUM_STATIC_EVENTS 16

extern HostDevice *host;
extern KeyboardEvent static_kbd_events[NUM_STATIC_EVENTS];
extern MouseEvent static_mouse_events[NUM_STATIC_EVENTS];

/* Convenience */
#define HOST_PROTOTYPES(NAME) \
extern void NAME##_init(); \
extern void NAME##_update(); \
extern void NAME##_kbd_event(const KeyboardEvent* events, uint8_t count); \
extern void NAME##_mouse_event(const MouseEvent* events, uint8_t count);

#define HOST_ENTRY(NAME)  { \
    #NAME, \
    NAME##_init, \
    NAME##_update, \
    NAME##_kbd_event, \
    NAME##_mouse_event \
}

void translate_boot_kbd_report(hid_keyboard_report_t const *report, HostDevice* host);
void translate_boot_mouse_report(hid_mouse_report_t const *report, HostDevice* host);

#endif