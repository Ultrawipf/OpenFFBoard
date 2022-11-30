/*
 * usb_descriptors.h
 *
 *  Created on: 16.02.2021
 *      Author: Yannick
 */

#ifndef USB_TINYUSB_USB_DESCRIPTORS_H_
#define USB_TINYUSB_USB_DESCRIPTORS_H_

#include "constants.h"
#include "tusb.h"
#include <string>
#include <vector>


#define HID_BINTERVAL 0x01 // 1 = 1000hz, 2 = 500hz, 3 = 333hz 4 = 250hz, 5 = 200hz 6 = 166hz, 7 = 125hz...

typedef struct usb_string_desc
{
	const uint16_t langId = 0x0904 ;
	const std::string manufacturer;
	const std::string product;
	const std::vector<std::string> interfaces;
} usb_string_desc_t;


/*
 * Device descriptors
 */
extern const tusb_desc_device_t usb_devdesc_ffboard_composite;

/*
 * Config descriptors
 */
extern const uint8_t usb_cdc_conf[];

#ifdef AXIS1_FFB_HID_DESC
extern const uint8_t usb_cdc_hid_conf_1axis[];
#endif

#ifdef AXIS2_FFB_HID_DESC
extern const uint8_t usb_cdc_hid_conf_2axis[];
#endif

#ifdef FFB_HID_DESC_GAMEPAD
extern const uint8_t usb_cdc_hid_conf_gamepad[];
#endif

extern const uint8_t usb_cdc_midi_conf[];

// Default strings
extern const usb_string_desc_t usb_ffboard_strings_default;

#endif /* USB_TINYUSB_USB_DESCRIPTORS_H_ */
