/*
 * usb_descriptors.h
 *
 *  Created on: 16.02.2021
 *      Author: Yannick
 */

#ifndef USB_TINYUSB_USB_DESCRIPTORS_H_
#define USB_TINYUSB_USB_DESCRIPTORS_H_


#include "tusb.h"
#include <string>
#include <vector>


#define HID_BINTERVAL 0x01

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
extern const uint8_t usb_cdc_hid_conf[];
extern const uint8_t usb_cdc_midi_conf[];

// Default strings
extern const usb_string_desc_t usb_ffboard_strings_default;

#endif /* USB_TINYUSB_USB_DESCRIPTORS_H_ */
