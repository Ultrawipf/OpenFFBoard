/*
 * usb_hid_ffb_desc.h
 *
 *  Created on: 22.02.2021
 *      Author: Yannick
 */

#ifndef USB_INC_USB_HID_FFB_DESC_H_
#define USB_INC_USB_HID_FFB_DESC_H_
#include "constants.h"

#if defined(HIDAXISRES_32B)
#define HIDDESC_32B_ENTRY(count)  /*     LOGICAL_MINIMUM (-7FFFFFFF)*/\
  0x17, 0x01, 0x00, 0x00, 0x80, \
  /*     LOGICAL_MAXIMUM (7FFFFFFF)*/\
  0x27, 0xff, 0xff, 0xff, 0x7f,\
 /*     REPORT_SIZE (16)*/\
  0x75, 0x20,\
  /* REPORT_COUNT */\
  0x95, count,\
/*     INPUT (Data,Var,Abs)*/\
  0x81, 0x02,
#define HIDDESC_32B_ENTRY_SIZE 12 // 12 bytes extra for additional 16b definition 2B count
#endif

#if defined(HIDAXISRES_32B)
#define USB_HID_1FFB_REPORT_DESC_SIZE 1196 + 16
#else
#define USB_HID_1FFB_REPORT_DESC_SIZE 1196
#endif
#ifdef AXIS1_FFB_HID_DESC
extern const uint8_t hid_1ffb_desc[USB_HID_1FFB_REPORT_DESC_SIZE];
#endif

#if defined(HIDAXISRES_32B)
#define USB_HID_2FFB_REPORT_DESC_SIZE 1215 + 16
#else
#define USB_HID_2FFB_REPORT_DESC_SIZE 1215
#endif

#ifdef AXIS2_FFB_HID_DESC
extern const uint8_t hid_2ffb_desc[USB_HID_2FFB_REPORT_DESC_SIZE];
#endif

#if defined(HIDAXISRES_32B)
#define USB_HID_GAMEPAD_REPORT_DESC_SIZE 176 + 16
#else
#define USB_HID_GAMEPAD_REPORT_DESC_SIZE 176
#endif
#ifdef FFB_HID_DESC_GAMEPAD
extern const uint8_t hid_gamepad_desc[USB_HID_GAMEPAD_REPORT_DESC_SIZE];
#endif

#endif /* USB_INC_USB_HID_FFB_DESC_H_ */
