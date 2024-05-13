/*
 * Adapted from TinyUSB examples (hid-app.c), which is licensed as follows:
 * 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 */

#include <hardware/uart.h>
#include <tusb.h>

#define DEBUG_TAG "usb"
#include "babelfish.h"

#define MAX_REPORT  4

static struct
{
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

// TinyUSB Callbacks
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance);
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

void tuh_hid_set_protocol_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t protocol)
{
  DBG("HID Set Protocol Complete %d:%d %d\r\n", dev_addr, instance, protocol);
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    DBG("HID: Failed to request to receive report!\r\n");
  }
}

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor() can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  DBG("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  // TinyUSB will always switch to boot protocol if possible. We may choose to switch
  // back if we can understand the descriptors.

  hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
  DBG("HID has %u reports \r\n", hid_info[instance].report_count);
  for (int i = 0; i < hid_info[instance].report_count; ++i) {
    const tuh_hid_report_info_t* info = &hid_info[instance].report_info[i];
    DBG("  Report %d: id=%d, usage_page=0x%x, usage=0x%x\r\n", i, info->report_id, info->usage_page, info->usage);
  }

  uint8_t proto = tuh_hid_get_protocol(dev_addr, instance);
  if (proto == HID_PROTOCOL_BOOT) {
    const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    DBG("HID using boot protocol, sub-protocol = %s (%d)\r\n", protocol_str[itf_protocol], itf_protocol);
  } else if (proto == HID_PROTOCOL_REPORT) {
    DBG("HID using report protocol\r\n");
  }

  // Create first report request
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    DBG("HID: Failed to request to receive report!\r\n");
  }
  DBG("HID: report requested for %d:%d\n", dev_addr, instance);
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  DBG("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  uint8_t const protocol = tuh_hid_get_protocol(dev_addr, instance);

  DBG("HID report (dev %d:%d, protocol %d itf_protocol %d) length %d\n", dev_addr, instance, protocol, itf_protocol, len);

  if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
      translate_boot_kbd_report((hid_keyboard_report_t const*) report);
  } else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
      translate_boot_mouse_report((hid_mouse_report_t const*) report);
  } else {
      // Generic report requires matching ReportID and contents with previous parsed report info
      DBG("===== Generic report!\n");
      process_generic_report(dev_addr, instance, report, len);
  }

/*
  if (ax == instance) {
    DBG("Switching to HID_REPORT_PROTOCOL\r\n");
    tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);
    ax = -1;
  } else if (bx == instance) {
    DBG("Switching to HID_REPORT_PROTOCOL\r\n");
    tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);
    bx = -1;
  } else {
*/

  // Continue to request to receive a report
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    DBG("HID: Failed to request to receive report!\r\n");
  }
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) dev_addr;

  uint8_t const rpt_count = hid_info[instance].report_count;
  tuh_hid_report_info_t* rpt_info_arr = hid_info[instance].report_info;
  tuh_hid_report_info_t* rpt_info = NULL;

  if (rpt_count == 1 && rpt_info_arr[0].report_id == 0) {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  } else {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the arrray
    for(uint8_t i=0; i<rpt_count; i++) {
      if (rpt_id == rpt_info_arr[i].report_id ) {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info) {
    // printf("Couldn't find the report info for this report !\r\n");
    return;
  }

  // For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
  // - Keyboard                     : Desktop, Keyboard
  // - Mouse                        : Desktop, Mouse
  // - Gamepad                      : Desktop, Gamepad
  // - Consumer Control (Media Key) : Consumer, Consumer Control
  // - System Control (Power key)   : Desktop, System Control
  // - Generic (vendor)             : 0xFFxx, xx
  if (rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP) {
    switch (rpt_info->usage) {
      case HID_USAGE_DESKTOP_KEYBOARD:
        // TU_LOG1("HID receive keyboard report\r\n");
        // Assume keyboard follow boot report layout
        translate_boot_kbd_report((hid_keyboard_report_t*) report);
        break;

      case HID_USAGE_DESKTOP_MOUSE:
        // TU_LOG1("HID receive mouse report\r\n");
        // Assume mouse follow boot report layout
        translate_boot_mouse_report((hid_mouse_report_t*) report);
        break;

      default:
        break;
    }
  }
}
