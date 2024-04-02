/*
 * usb_hid_gamepad.c
 *
 *  Created on: 30.11.2022
 *      Author: Yannick
 */

#include "cppmain.h"
#include "ffb_defs.h"
#include "usb_hid_ffb_desc.h"

#ifdef FFB_HID_DESC_GAMEPAD

/**
 * USB HID descriptor containing a gamepad definition and the vendor defined reports but no PID FFB
 */
__ALIGN_BEGIN const uint8_t hid_gamepad_desc[USB_HID_GAMEPAD_REPORT_DESC_SIZE] __ALIGN_END =
{
		0x05, 0x01,                    /* USAGE_PAGE (Generic Desktop)*/
			0x09, 0x04,                    /* USAGE (Joystick)*/
			0xa1, 0x01,                    /* COLLECTION (Application)*/
		HIDDESC_GAMEPAD_16B,

		HIDDESC_CTRL_REPORTS, // HID command report support

  0xC0    /*     END_COLLECTION	             */
};

#endif

#ifdef FFB_HID_DESC_GAMEPAD_32B

/**
 * USB HID descriptor containing a gamepad definition and the vendor defined reports but no PID FFB
 */
__ALIGN_BEGIN const uint8_t hid_gamepad_desc_32b[USB_HID_GAMEPAD_REPORT_DESC_32B_SIZE] __ALIGN_END =
{
		0x05, 0x01,                    /* USAGE_PAGE (Generic Desktop)*/
			0x09, 0x04,                    /* USAGE (Joystick)*/
			0xa1, 0x01,                    /* COLLECTION (Application)*/
		HIDDESC_GAMEPAD_32B,

		HIDDESC_CTRL_REPORTS, // HID command report support

  0xC0    /*     END_COLLECTION	             */
};

#endif
