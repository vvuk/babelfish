#include <sys/cdefs.h>
#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>

_Noreturn void mainloop();
void select_output(uint output);

// pin 4-7.
static const uint path_sel_gpio[2] = {2, 3};
static const uint out_gpio = 4; // also UART 1 TX
static const uint in_gpio = 5; // also UART 2 RX
// pin 9.
static const uint en_gpio = 6;

// hookup is:
// pin 4 + 5 to sel0/1
// pin 9 to OE
// pin 6 is output, to CH_A
// CH_A0..3 to Saleae
// pin 7 is input, to CH_B
// CH_B0..3 to USB-serial tx

static bool s_have_chars = false;

void select_output(uint output)
{
    gpio_put(en_gpio, 1);
    gpio_put(path_sel_gpio[0], output & 1);
    gpio_put(path_sel_gpio[1], (output >> 1) & 1);
    gpio_put(en_gpio, 0);
}

const uint32_t wave_interval_us = 500000; // 500ms (2Hz)

_Noreturn void mainloop()
{
    uint32_t next_swap_time = time_us_32() + wave_interval_us;
    char out_char = 'A';

    while (true) {
        uint32_t now = time_us_32();
        if (now >= next_swap_time) {
            uart_putc_raw(uart1, out_char);
            if (out_char == 'Z') {
                out_char = 'A';
            } else {
                out_char++;
            }

            next_swap_time += wave_interval_us;
        }

        while (uart_is_readable(uart1)) {
            char c = uart_getc(uart1);
            printf("uart1: %c\n", c);
        }

        while (s_have_chars) {
            s_have_chars = false;
            char c = getchar();
            if (c >= '0' && c <= '3') {
                select_output(c - '0');
                printf("output selected: %c\n", c);
            } else if (c == '\n') {
                printf("hardware test: enter 0 1 2 3 to select channel 0 1 2 3\n");
            }
        }
    }
}

void chars_avail_cb(void* val)
{
    s_have_chars = true;
}

#define GP_OUT(k) do { gpio_init(k); gpio_set_dir(k, GPIO_OUT); gpio_put(k, false); } while (0)

int main(void)
{
    // need 120MHz for USB
    set_sys_clock_khz(120000, true);

    stdio_usb_init();

    while (!stdio_usb_connected()) {}

    GP_OUT(2);
    GP_OUT(3);
    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);
    GP_OUT(6);

    GP_OUT(25); // led
    gpio_set_drive_strength(24, GPIO_DRIVE_STRENGTH_4MA);
    gpio_put(25, 1);

    gpio_put(en_gpio, 1);

    uart_init(uart1, 38400);

    stdio_set_chars_available_callback(chars_avail_cb, NULL);

    printf("\n");
    printf("\n");
    printf("\n");
    printf("hardware test: enter 0 1 2 3 to select channel 0 1 2 3\n");

    mainloop();

    return 0;
}
