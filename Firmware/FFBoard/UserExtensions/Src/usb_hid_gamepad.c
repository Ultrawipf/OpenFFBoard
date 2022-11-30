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
		   0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
		    0x09, 0x04,                    // USAGE (Joystick)
		    0xa1, 0x01,                    // COLLECTION (Application)
		    0xa1, 0x00,                    //   COLLECTION (Physical)
		    0x85, 0x01,                    //     REPORT_ID (1)
		    0x05, 0x09,                    //     USAGE_PAGE (Button)
		    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
		    0x29, 0x40,                    //     USAGE_MAXIMUM (Button 64)
		    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
		    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
		    0x95, 0x40,                    //     REPORT_COUNT (64)
		    0x75, 0x01,                    //     REPORT_SIZE (1)
		    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
		    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
		    0x09, HID_USAGE_X,             //     USAGE (X)
		    0x09, HID_USAGE_Y,             //     USAGE (Y)
		    0x09, HID_USAGE_Z,             //     USAGE (Z)
		    0x09, HID_USAGE_RX,            //     USAGE (Rx)
		    0x09, HID_USAGE_RY,            //     USAGE (Ry)
			0x09, HID_USAGE_RZ,            //     USAGE (Rz)
			0x09, HID_USAGE_SL1,           //     USAGE (Dial)
			0x09, HID_USAGE_SL0,           //     USAGE (Slider)
		    0x16, 0x01, 0x80,              //     LOGICAL_MINIMUM (-32767)
		    0x26, 0xff, 0x7f,              //     LOGICAL_MAXIMUM (32767)
		    0x75, 0x10,                    //     REPORT_SIZE (16)
		    0x95, 0x08,                    //     REPORT_COUNT (8)
		    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
		    0xc0,                          //   END_COLLECTION


			// Control reports
			0x06, 0x00, 0xFF,                    // USAGE_PAGE (Vendor)
			0x09, 0x00,                    //   USAGE (Vendor)
			0xA1, 0x01, // Collection (Application)

				0x85,HID_ID_HIDCMD, //    Report ID
				0x09, 0x01,                    //   USAGE (Vendor)
				0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
				0x26, 0x04,	0x00,			   //   Logical Maximum 4
				0x75, 0x08,                    //   REPORT_SIZE (8)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

				0x09, 0x02,                    //   USAGE (Vendor) class address
				0x75, 0x10,                    //   REPORT_SIZE (16)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

				0x09, 0x03,                    //   USAGE (Vendor) class instance
				0x75, 0x08,                    //   REPORT_SIZE (8)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

				0x09, 0x04,                    //   USAGE (Vendor) cmd
				0x75, 0x20,                    //   REPORT_SIZE (32)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

				0x09, 0x05,                    //   USAGE (Vendor)
				0x75, 0x40,                    //   REPORT_SIZE (64) value
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

				0x09, 0x06,                    //   USAGE (Vendor) address
				0x75, 0x40,                    //   REPORT_SIZE (64)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

				0x85,HID_ID_HIDCMD, //    Report ID
				0x09, 0x01,                    //   USAGE (Vendor)
				0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
				0x26, 0x04,	0x00,			   //   Logical Maximum 4
				0x75, 0x08,                    //   REPORT_SIZE (8)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x81, 0x02,                    //   INPUT (Data,Var,Abs)

				0x09, 0x02,                    //   USAGE (Vendor) class address
				0x75, 0x10,                    //   REPORT_SIZE (16)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x81, 0x02,                    //   INPUT (Data,Var,Abs)

				0x09, 0x03,                    //   USAGE (Vendor) class instance
				0x75, 0x08,                    //   REPORT_SIZE (8)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x81, 0x02,                    //   INPUT (Data,Var,Abs)

				0x09, 0x04,                    //   USAGE (Vendor) cmd
				0x75, 0x20,                    //   REPORT_SIZE (32)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x81, 0x02,                    //   INPUT (Data,Var,Abs)

				0x09, 0x05,                    //   USAGE (Vendor)
				0x75, 0x40,                    //   REPORT_SIZE (64) value
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x81, 0x02,                    //   INPUT (Data,Var,Abs)

				0x09, 0x06,                    //   USAGE (Vendor) address
				0x75, 0x40,                    //   REPORT_SIZE (64)
				0x95, 0x01,                    //   REPORT_COUNT (1)
				0x81, 0x02,                    //   INPUT (Data,Var,Abs)



		  0xc0,                          //   END_COLLECTION


  0xC0    /*     END_COLLECTION	             */
};

#endif
