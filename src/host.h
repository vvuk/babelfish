#ifndef HOST_H
#define HOST_H

#include <stdint.h>
#include <pico/mutex.h>
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
    void (*kbd_event)(const KeyboardEvent events);
    void (*mouse_event)(const MouseEvent events);
} HostDevice;

typedef struct uart_inst uart_inst_t;

typedef struct {
    uart_inst_t *uart;
    uint uart_irq;
    uint uart_rx_gpio;
    uint uart_tx_gpio;
} OutputUartDevice;

#define EVENT_IS_HOST_MOD(event) (event.keycode == HID_KEY_LEFT_GUI || event.keycode == HID_KEY_RIGHT_GUI || event.keycode == HID_KEY_RIGHT_ALT)


extern HostDevice *host;
extern OutputUartDevice out_uart;

#define MAX_QUEUED_EVENTS 32

void enqueue_kbd_event(const KeyboardEvent* event);
void enqueue_mouse_event(const MouseEvent* event);
void get_queued_kbd_events(KeyboardEvent* events, uint* count);
void get_queued_mouse_events(MouseEvent* events, uint* count);

/* Convenience */
#define HOST_PROTOTYPES(NAME) \
extern void NAME##_init(); \
extern void NAME##_update(); \
extern void NAME##_kbd_event(const KeyboardEvent event); \
extern void NAME##_mouse_event(const MouseEvent event);

#define HOST_ENTRY(NAME)  { \
    #NAME, \
    NAME##_init, \
    NAME##_update, \
    NAME##_kbd_event, \
    NAME##_mouse_event \
}

void babelfish_uart_config(int uidx, char ab);

void translate_boot_kbd_report(hid_keyboard_report_t const *report, HostDevice* host);
void translate_boot_mouse_report(hid_mouse_report_t const *report, HostDevice* host);

#endif