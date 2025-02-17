/*
 * usb_hid_ffb_desc.h
 *
 *  Created on: 22.02.2021
 *      Author: Yannick
 */

#ifndef USB_INC_USB_HID_FFB_DESC_H_
#define USB_INC_USB_HID_FFB_DESC_H_
#include "constants.h"


#define USB_HID_1FFB_REPORT_DESC_SIZE 1196
#ifdef AXIS1_FFB_HID_DESC
extern const uint8_t hid_1ffb_desc[USB_HID_1FFB_REPORT_DESC_SIZE];
#endif

#define USB_HID_2FFB_REPORT_DESC_SIZE 1215//1213
#ifdef AXIS2_FFB_HID_DESC
extern const uint8_t hid_2ffb_desc[USB_HID_2FFB_REPORT_DESC_SIZE];
#endif

#define USB_HID_GAMEPAD_REPORT_DESC_SIZE 176
#ifdef FFB_HID_DESC_GAMEPAD
extern const uint8_t hid_gamepad_desc[USB_HID_GAMEPAD_REPORT_DESC_SIZE];
#endif

#endif /* USB_INC_USB_HID_FFB_DESC_H_ */
