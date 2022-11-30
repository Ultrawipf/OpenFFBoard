/*
 * usb_descriptors.c
 *
 *  Created on: 16.02.2021
 *      Author: Yannick
 */
#include "tusb.h"
#include "usb_descriptors.h"
#include "usbd.h"
#include "stm32f4xx_hal.h"
#include "main.h"
#include "usb_hid_ffb_desc.h"

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
#define USBD_VID     0x1209
#define USBD_PID     0xFFB0
const tusb_desc_device_t usb_devdesc_ffboard_composite =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USBD_VID,
    .idProduct          = USBD_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};


/*--------------------------------------------------------------------
 * Configuration descriptors
 * String index starts at 4 (interfaces field)
----------------------------------------------------------------------*/

const uint8_t usb_cdc_conf[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 2, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(0, 4, 0x82, 8, 0x01, 0x81, 64),
};

// Composite CDC and HID
#ifdef AXIS1_FFB_HID_DESC
const uint8_t usb_cdc_hid_conf_1axis[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 3, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_INOUT_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(0, 4, 0x82, 8, 0x01, 0x81, 64),

  // HID Descriptor. EP 83 and 2
  TUD_HID_INOUT_DESCRIPTOR(2, 5, HID_ITF_PROTOCOL_NONE, USB_HID_1FFB_REPORT_DESC_SIZE, 0x83, 0x02, 64, HID_BINTERVAL),
};
#endif

// Composite CDC and HID
#ifdef AXIS2_FFB_HID_DESC
const uint8_t usb_cdc_hid_conf_2axis[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 3, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_INOUT_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(0, 4, 0x82, 8, 0x01, 0x81, 64),

  // HID Descriptor. EP 83 and 2
  TUD_HID_INOUT_DESCRIPTOR(2, 5, HID_ITF_PROTOCOL_NONE, USB_HID_2FFB_REPORT_DESC_SIZE, 0x83, 0x02, 64, HID_BINTERVAL),
};
#endif

// Composite CDC and HID
#ifdef FFB_HID_DESC_GAMEPAD
const uint8_t usb_cdc_hid_conf_gamepad[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 3, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_INOUT_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(0, 4, 0x82, 8, 0x01, 0x81, 64),

  // HID Descriptor. EP 83 and 2
  TUD_HID_INOUT_DESCRIPTOR(2, 5, HID_ITF_PROTOCOL_NONE, USB_HID_GAMEPAD_REPORT_DESC_SIZE, 0x83, 0x02, 64, HID_BINTERVAL),
};
#endif

// Composite CDC and MIDI
uint8_t const usb_cdc_midi_conf[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 4, 0, TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MIDI_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(0, 4, 0x82, 8, 0x01, 0x81, 64),
  // Interface number, string index, EP Out & EP In address, EP size
  TUD_MIDI_DESCRIPTOR(2, 6, 0x02, 0x83, 64)
};

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// Default ffboard names
const usb_string_desc_t usb_ffboard_strings_default = {
	.langId = 0x0409,
	.manufacturer = "Open FFBoard",
	.product = "FFBoard",
	// Interfaces start at index 4
	.interfaces = {"FFBoard CDC", "FFBoard HID","FFBoard MIDI"}
};


