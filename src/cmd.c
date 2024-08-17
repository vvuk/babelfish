#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <tusb.h>
#include <pio_usb.h>

#define DEBUG_VERBOSE 0
#define DEBUG_TAG "cmd"

#include "babelfish.h"

#define CMD_MS_HOLD 500
#define CMD_KEY HID_KEY_EQUAL

static KeyboardEvent s_cmd_saved_ev;
static uint32_t s_cmd_down_stamp = 0;
static bool s_in_cmd = false;

char hid_to_cmd_ascii(uint16_t hid)
{
    if (hid >= HID_KEY_0 && hid <= HID_KEY_9)
        return '0' + (hid - HID_KEY_0);

    if (hid >= HID_KEY_A && hid <= HID_KEY_Z)
        return 'a' + (hid - HID_KEY_A);

    if (hid == HID_KEY_ENTER)
        return '\n';

    if (hid == HID_KEY_SPACE)
        return ' ';

    return 0;
}

uint16_t cmd_ascii_to_hid(char ch)
{
    if (ch >= '0' && ch <= '9')
        return HID_KEY_0 + (ch - '0');

    if (ch >= 'a' && ch <= 'z')
        return HID_KEY_A + (ch - 'a');

    if (ch == '\n')
        return HID_KEY_ENTER;

    if (ch == ' ')
        return HID_KEY_SPACE;

    return 0;
}

void send_kbd_string(const char* str)
{
    while (*str) {
        uint16_t hid = cmd_ascii_to_hid(*str++);
        if (hid == 0)
            continue;

        KeyboardEvent ev = { .page = 0, .keycode = hid, .down = true };
        host->kbd_event(ev);
        sleep_ms(100);
        ev.down = false;
        host->kbd_event(ev);
        sleep_ms(100);
    }
}

void cmd_process_char(char c)
{
    static char cmd[12];
    static char cmd_len = 0;

    if (c == 0) {
        cmd_len = 0;
        return;
    }

    cmd[cmd_len++] = c;

    if (cmd_len == 1) {
        switch (c) {
            case 'h':
                send_kbd_string("Hosts\n");
                int i = 0;
                while (hosts[i].name) {
                    char buf[32];
                    snprintf(buf, 32, "%d", i);
                    send_kbd_string(g_current_host_index == i ? "* " : "  ");
                    send_kbd_string(buf);
                    send_kbd_string(" ");
                    send_kbd_string(hosts[i].name);
                    send_kbd_string("\n");
                    i++;
                }
                cmd_len = 0;
                break;

            default:
                break;
        }
    } else if (cmd_len == 5) {
        if (cmd[0] == 'd') {
            bool ch_num = cmd[1] - 'a';
            bool is_high = cmd[2] == '1';
            bool is_normal = cmd[3] == 'n';
            int mode = cmd[4] - '1';
            if (ch_num < 0 || ch_num > 1) ch_num = 0;
            if (mode < 0 || mode > 2) mode = 0;

            channel_config(ch_num,
                    ChannelModeGPIO |
                    (is_normal ? ChannelModeNoInvert : ChannelModeInvert) |
                    mode);
            gpio_put(ch_num ? TX_A_GPIO : TX_B_GPIO, is_high);
        }
    }
}

bool cmd_process_event(KeyboardEvent ev)
{
    if (s_cmd_down_stamp != 0) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms - s_cmd_down_stamp < CMD_MS_HOLD) {
            // nope, key wasn't held down long enough.
            // process the original key down and then continue
            host->kbd_event(s_cmd_saved_ev);
            s_cmd_down_stamp = 0;
            return false;
        }

        s_in_cmd = true;
        cmd_process_char(0); // reset the command processor
    }

    if (s_in_cmd) {
        // check for ending (on release)
        if (ev.keycode == CMD_KEY && !ev.down) {
            s_cmd_down_stamp = 0;
            s_in_cmd = false;
            return true;
        }

        // ignore key releases
        if (ev.down) {
            cmd_process_char(hid_to_cmd_ascii(ev.keycode));
        }
        return true;
    }

    if (ev.keycode == CMD_KEY && ev.down) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        s_cmd_down_stamp = now_ms;
        s_cmd_saved_ev = ev;
        return true;
    }

    return false;
}

