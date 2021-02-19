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
#include "stdlib.h"


/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]       MIDI | HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum
{
  ITF_NUM_CDC_0 = 0,
  ITF_NUM_CDC_0_DATA,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + 1 * TUD_CDC_DESC_LEN)


  #define EPNUM_CDC_0_NOTIF   0x82
  #define EPNUM_CDC_0_DATA    0x01

  #define EPNUM_CDC_1_NOTIF   0x84
  #define EPNUM_CDC_1_DATA    0x03

uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_DATA, 0x80 | EPNUM_CDC_0_DATA, 64),

//  // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
//  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 4, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_DATA, 0x80 | EPNUM_CDC_1_DATA, 64),
};

#if TUD_OPT_HIGH_SPEED
uint8_t const desc_hs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_DATA, 0x80 | EPNUM_CDC_0_DATA, 512),

  // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 4, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_DATA, 0x80 | EPNUM_CDC_1_DATA, 512),
};
#endif

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ?  desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "TinyUSB",                     // 1: Manufacturer
  "TinyUSB Device",              // 2: Product
  "123456",                      // 3: Serials, should use chip ID
  "TinyUSB CDC",                 // 4: CDC Interface
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

  return _desc_str;
}

//// ----------------------------------------------------------------------------
//tusb_desc_device_t const desc_device_cdc =
//{
//    .bLength            = sizeof(tusb_desc_device_t),
//    .bDescriptorType    = TUSB_DESC_DEVICE,
//    .bcdUSB             = 0x0200,
//
//    // Use Interface Association Descriptor (IAD) for CDC
//    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
//    .bDeviceClass       = TUSB_CLASS_MISC,
//    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
//    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
//    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
//
//    .idVendor           = 0x1209,
//    .idProduct          = 0xFFB0,
//    .bcdDevice          = 0x0100,
//
//    .iManufacturer      = 0x01,
//    .iProduct           = 0x02,
//    .iSerialNumber      = 0x03,
//
//    .bNumConfigurations = 0x01
//};
//uint8_t const * tud_descriptor_device_cb(void)
//{
//  return (uint8_t const *) &desc_device_cdc;
//}
//
//enum
//{
//  ITF_NUM_CDC = 0,
//  ITF_NUM_CDC_DATA,
//  ITF_NUM_TOTAL
//};
//uint8_t const desc_fs_configuration[] =
//{
//  // Config number, interface count, string index, total length, attribute, power in mA
//  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
//
//  // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
//  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, 0x82, 8, 0x01, 0x82, 64)
//};
//uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
//{
//  (void) index; // for multiple configurations
//
//return desc_fs_configuration;
//}
//
//
//// ----------------------------------------------------------------------------
//
//char serialNum[11] = {0};
//const char * USB_generate_serial()
//{
//	// Will roll over but generates a likely unique id
//	uint32_t serial = HAL_GetUIDw0() + HAL_GetUIDw1() + HAL_GetUIDw2();
//	utoa(serial,serialNum,10);
//	return serialNum;
//}
//
//
//char const* string_desc_arr [] =
//{
//  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
//  "Open FFBoard",                     // 1: Manufacturer
//  "Open FFBoard",              // 2: Product
//  "1337",                      // 3: Serials, should use chip ID
//  "FFBoard CDC",                 // 4: CDC Interface
//  "FFBoard HID",                 // 5: HID Interface
//};
//
//
//static uint16_t _desc_str[64];
//
//// Invoked when received GET STRING DESCRIPTOR request
//// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
//uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
//{
//  (void) langid;
//
//  uint8_t chr_count;
//
//  if ( index == 0)
//  {
//    memcpy(&_desc_str[1], string_desc_arr[0], 2);
//    chr_count = 1;
//  }else
//  {
//    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
//    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
//
//    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;
//
//    const char* str = string_desc_arr[index];
//
//    // Cap at max char
//    chr_count = strlen(str);
//    if ( chr_count > 31 ) chr_count = 31;
//
//    // Convert ASCII string into UTF-16
//    for(uint8_t i=0; i<chr_count; i++)
//    {
//      _desc_str[1+i] = str[i];
//    }
//  }
//
//  // first byte is length (including header), second byte is string type
//  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);
//
//  return _desc_str;
//}
