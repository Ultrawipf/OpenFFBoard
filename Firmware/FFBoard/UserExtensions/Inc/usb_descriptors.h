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
 * Base combined usb config descriptor for HID and CDC composite device
 */
#define USB_CONF_DESC_HID_CDC(HIDREPSIZE,EPSIZE) \
		  /* Config number, interface count, string index, total length, attribute, power in mA*/\
		  TUD_CONFIG_DESCRIPTOR(1, 3, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_INOUT_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP | TUSB_DESC_CONFIG_ATT_SELF_POWERED, 100),\
		  /* 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.*/\
		  TUD_CDC_DESCRIPTOR(0, 4, 0x82, 8, 0x01, 0x81, EPSIZE),\
		  /* HID Descriptor. EP 83 and 2*/\
		  TUD_HID_INOUT_DESCRIPTOR(2, 5, HID_ITF_PROTOCOL_NONE, HIDREPSIZE, 0x83, 0x02,EPSIZE, HID_BINTERVAL)

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
#ifdef AXIS2_FFB_HID_DESC_32B
extern const uint8_t usb_cdc_hid_conf_2axis_32b[];
#endif
#ifdef FFB_HID_DESC_GAMEPAD
extern const uint8_t usb_cdc_hid_conf_gamepad[];
#endif

extern const uint8_t usb_cdc_midi_conf[];

// Default strings
extern const usb_string_desc_t usb_ffboard_strings_default;

#endif /* USB_TINYUSB_USB_DESCRIPTORS_H_ */
