#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <tusb.h>
#include <stdarg.h>
#include <pio_usb.h>

#include "babelfish.h"

#define DEBUG_TAG "main"

#include "host.h"
#include "debug.h"

// English
#define LANGUAGE_ID 0x0409
#define BUF_COUNT   4

#define USB_CONTROLLER 1

tusb_desc_device_t desc_device;

uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = { 0 }; // device address that owns buffer

void debug_init(void);
void led_blinking_task(void);
void core1_main(void);

void test_u1_3v3(void);
void test_u1_vcc(void);
void test2(void);
void test_irq_handler();

static void print_utf16(uint16_t *temp_buf, size_t buf_len);
void print_device_descriptor(tuh_xfer_t* xfer);

void
dbg(const char* tag, const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    printf("[%s] %s\n", tag, buf);
#if false
    char *bp = buf;
    uart_putc_raw(UART_DEBUG_ID, '(');
    while (*tag) {
        uart_putc_raw(UART_DEBUG_ID, *tag++);
    }
    uart_putc_raw(UART_DEBUG_ID, ')');
    uart_putc_raw(UART_DEBUG_ID, ' ');

    while (*bp) {
        if (*bp == '\n') {
            uart_putc_raw(UART_DEBUG_ID, '\r');
        }
        uart_putc_raw(UART_DEBUG_ID, *bp++);
    }
#endif
}

//--------------------------------------------------------------------+
// String Descriptor Helper
//--------------------------------------------------------------------+

static void _convert_utf16le_to_utf8(const uint16_t *utf16, size_t utf16_len, uint8_t *utf8, size_t utf8_len) {
    // TODO: Check for runover.
    (void)utf8_len;
    // Get the UTF-16 length out of the data itself.

    for (size_t i = 0; i < utf16_len; i++) {
        uint16_t chr = utf16[i];
        if (chr < 0x80) {
            *utf8++ = chr & 0xffu;
        } else if (chr < 0x800) {
            *utf8++ = (uint8_t)(0xC0 | (chr >> 6 & 0x1F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
        } else {
            // TODO: Verify surrogate.
            *utf8++ = (uint8_t)(0xE0 | (chr >> 12 & 0x0F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 6 & 0x3F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
        }
        // TODO: Handle UTF-16 code points that take two entries.
    }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t *buf, size_t len) {
    size_t total_bytes = 0;
    for (size_t i = 0; i < len; i++) {
        uint16_t chr = buf[i];
        if (chr < 0x80) {
            total_bytes += 1;
        } else if (chr < 0x800) {
            total_bytes += 2;
        } else {
            total_bytes += 3;
        }
        // TODO: Handle UTF-16 code points that take two entries.
    }
    return (int) total_bytes;
}

static void print_utf16(uint16_t *temp_buf, size_t buf_len) {
    size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
    size_t utf8_len = (size_t) _count_utf8_bytes(temp_buf + 1, utf16_len);
    _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t *) temp_buf, sizeof(uint16_t) * buf_len);
    ((uint8_t*) temp_buf)[utf8_len] = '\0';

    printf((char*)temp_buf);
}
// get an buffer from pool
uint8_t* get_hid_buf(uint8_t daddr)
{
  for(size_t i=0; i<BUF_COUNT; i++)
  {
    if (buf_owner[i] == 0)
    {
      buf_owner[i] = daddr;
      return buf_pool[i];
    }
  }

  // out of memory, increase BUF_COUNT
  return NULL;
}

// free all buffer owned by device
void free_hid_buf(uint8_t daddr)
{
  for(size_t i=0; i<BUF_COUNT; i++)
  {
    if (buf_owner[i] == daddr) buf_owner[i] = 0;
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  DBG("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  DBG("HID device address = %d, instance = %d report %*s\r\n", dev_addr, instance, len, report);
}

// Invoked when device is mounted (configured)
// looked up via weak symbol reference, fun!
void tuh_mount_cb (uint8_t daddr)
{
  printf("Device attached, address = %d\r\n", daddr);

  // Get Device Descriptor
  // TODO: invoking control transfer now has issue with mounting hub with multiple devices attached, fix later
  tuh_descriptor_get_device(daddr, &desc_device, 18, print_device_descriptor, 0);
}

/// Invoked when device is unmounted (bus reset/unplugged)
void tuh_umount_cb(uint8_t daddr)
{
  printf("Device removed, address = %d\r\n", daddr);
  free_hid_buf(daddr);
}

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+

void print_device_descriptor(tuh_xfer_t* xfer)
{
  if ( XFER_RESULT_SUCCESS != xfer->result )
  {
    printf("Failed to get device descriptor\r\n");
    return;
  }

  uint8_t const daddr = xfer->daddr;

  printf("Device %u: ID %04x:%04x\r\n", daddr, desc_device.idVendor, desc_device.idProduct);
  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n"     , desc_device.bLength);
  printf("  bDescriptorType     %u\r\n"     , desc_device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n"   , desc_device.bcdUSB);
  printf("  bDeviceClass        %u\r\n"     , desc_device.bDeviceClass);
  printf("  bDeviceSubClass     %u\r\n"     , desc_device.bDeviceSubClass);
  printf("  bDeviceProtocol     %u\r\n"     , desc_device.bDeviceProtocol);
  printf("  bMaxPacketSize0     %u\r\n"     , desc_device.bMaxPacketSize0);
  printf("  idVendor            0x%04x\r\n" , desc_device.idVendor);
  printf("  idProduct           0x%04x\r\n" , desc_device.idProduct);
  printf("  bcdDevice           %04x\r\n"   , desc_device.bcdDevice);

  // Get String descriptor using Sync API
  uint16_t temp_buf[128];

  printf("  iManufacturer       %u     "     , desc_device.iManufacturer);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)) )
  {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\r\n");

  printf("  iProduct            %u     "     , desc_device.iProduct);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
  {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\r\n");

  printf("  iSerialNumber       %u     "     , desc_device.iSerialNumber);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
  {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\r\n");

  printf("  bNumConfigurations  %u\r\n"     , desc_device.bNumConfigurations);

  // Get configuration descriptor with sync API
  //if (XFER_RESULT_SUCCESS == tuh_descriptor_get_configuration_sync(daddr, 0, temp_buf, sizeof(temp_buf)))
  //{
  //  parse_config_descriptor(daddr, (tusb_desc_configuration_t*) temp_buf);
  //}
}


int main(void) {
  // need 120MHz for USB
  set_sys_clock_khz(120000, true);

  stdio_init_all();
  //stdio_semihosting_init();

  //sleep_ms(10);

  DEBUG_INIT();

  DBG("Initialized babelfish_test\n");

#if false
  // Initialize Core 1, and put PIO-USB on it with TinyUSB
  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  board_init();

  // tinyusb listen for device connection

  while (true) {
    //led_blinking_task();
    sleep_ms(1000);
    //printf("Hello world\n");
  }
#else
  core1_main();
#endif
  return 0;
}

void
debug_init()
{
    gpio_set_function(UART_DEBUG_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_DEBUG_RX_PIN, GPIO_FUNC_UART);
    uart_init(UART_DEBUG_ID, 115200);
    uart_set_hw_flow(UART_DEBUG_ID, false, false);
    uart_set_format(UART_DEBUG_ID, 8, 1, UART_PARITY_NONE);

    irq_set_exclusive_handler(UART_DEBUG_IRQ, test_irq_handler);
    irq_set_enabled(UART_DEBUG_IRQ, true);
    uart_set_irq_enables(UART_DEBUG_ID, true, false);
}

void
test_irq_handler()
{
    while (uart_is_readable(UART_DEBUG_ID)) {
        uint8_t ch = uart_getc(UART_DEBUG_ID);
    }
}

void led_blinking_task(void)
{
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//
// Core 1
//
void core1_main(void) {
  //sleep_ms(10);

  // Use tuh_configure() to pass pio configuration to the host stack
  // Note: tuh_configure() must be called before
  pio_usb_configuration_t pio_cfg = {
    .pin_dp = 29,                     // gpio29 is DP on babelfish, 28 is DM
    .pio_tx_num = PIO_USB_TX_DEFAULT, // 0
    .sm_tx = PIO_SM_USB_TX_DEFAULT,   // 0
    .tx_ch = PIO_USB_DMA_TX_DEFAULT,  // 0
    .pio_rx_num = PIO_USB_RX_DEFAULT, // 1
    .sm_rx = PIO_SM_USB_RX_DEFAULT,   // 0
    .sm_eop = PIO_SM_USB_EOP_DEFAULT, // 1
    .alarm_pool = NULL,
    .debug_pin_rx = (-1),
    .debug_pin_eop = (-1),
    //.skip_alarm_pool = false
  };

  if (!tuh_configure(USB_CONTROLLER, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg)) {
    printf("tuh_configure failed\n");
    return;
  }

  // To run USB SOF interrupt in core1, init host stack for pio_usb (roothub
  // port1) on core1
  if (!tuh_init(USB_CONTROLLER)) {
    printf("tuh_init failed\n");
    return;
  }

  while (true) {
    tuh_task(); // tinyusb host task
  }
}

void test_u1_3v3(void)
{
}

void test_u1_vcc(void)
{
}

void test2()
{
}
