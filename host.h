#ifndef HOST_H
#define HOST_H

#include <stdint.h>

#include <tusb.h>

typedef struct {
    char name[16];
    void (*init)();
    void (*update)();
    void (*kbd_report)(hid_keyboard_report_t const *report);
    void (*mouse_report)(hid_mouse_report_t const *report);
} HostDevice;

extern HostDevice *host;

/* Convenience */
#define HOST_PROTOTYPES(NAME) \
extern void NAME##_init(); \
extern void NAME##_update(); \
extern void NAME##_kbd_report(hid_keyboard_report_t const * report); \
extern void NAME##_mouse_report(hid_mouse_report_t const * report)

#define HOST_ENTRY(NAME)  { \
    #NAME, \
    NAME##_init, \
    NAME##_update, \
    NAME##_kbd_report, \
    NAME##_mouse_report \
}


#endif