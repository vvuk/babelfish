#ifndef HOST_H
#define HOST_H

#include <stdint.h>

typedef enum {
    // Don't call kbd/mouse event functions,
    // the host will do its own pumping
    HostFlagDontPumpEvents = 1 << 0,
} HostFlags;

typedef struct {
    char name[16];
    void (*init)();
    void (*update)();
    void (*kbd_event)(const KeyboardEvent events);
    void (*mouse_event)(const MouseEvent events);
} HostDevice;

extern HostDevice *host;

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

#endif