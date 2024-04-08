/*
 * usb_hid_ffb_desc.h
 *
 *  Created on: 22.02.2021
 *      Author: Yannick
 */

#ifndef USB_INC_USB_HID_FFB_DESC_H_
#define USB_INC_USB_HID_FFB_DESC_H_
#include "constants.h"
#include "hid.h"

/** \defgroup HID Descriptors
 *  \details Contains definitions of different HID descriptor parts *
 *  @{
 */



// HID descriptor building blocks
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


#define HIDDESC_GAMEPAD_16B \
		    0xa1, 0x00,                    /*   COLLECTION (Physical)*/\
		    0x85, 0x01,                    /*     REPORT_ID (1)*/\
		    0x05, 0x09,                    /*     USAGE_PAGE (Button)*/\
		    0x19, 0x01,                    /*     USAGE_MINIMUM (Button 1)*/\
		    0x29, 0x40,                    /*     USAGE_MAXIMUM (Button 64)*/\
		    0x15, 0x00,                    /*     LOGICAL_MINIMUM (0)*/\
		    0x25, 0x01,                    /*     LOGICAL_MAXIMUM (1)*/\
		    0x95, 0x40,                    /*     REPORT_COUNT (64)*/\
		    0x75, 0x01,                    /*     REPORT_SIZE (1)*/\
		    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)*/\
		    0x05, 0x01,                    /*     USAGE_PAGE (Generic Desktop)*/\
		    0x09, HID_USAGE_DESKTOP_X,             /*     USAGE (X)*/\
		    0x09, HID_USAGE_DESKTOP_Y,             /*     USAGE (Y)*/\
		    0x09, HID_USAGE_DESKTOP_Z,             /*     USAGE (Z)*/\
		    0x09, HID_USAGE_DESKTOP_RX,            /*     USAGE (Rx)*/\
		    0x09, HID_USAGE_DESKTOP_RY,            /*     USAGE (Ry)*/\
			0x09, HID_USAGE_DESKTOP_RZ,            /*     USAGE (Rz)*/\
			0x09, HID_USAGE_DESKTOP_DIAL,           /*     USAGE (Dial)*/\
			0x09, HID_USAGE_DESKTOP_SLIDER,           /*     USAGE (Slider)*/\
		    0x16, 0x01, 0x80,              /*     LOGICAL_MINIMUM (-32767)*/\
		    0x26, 0xff, 0x7f,              /*     LOGICAL_MAXIMUM (32767)*/\
		    0x75, 0x10,                    /*     REPORT_SIZE (16)*/\
		    0x95, 0x08,                    /*     REPORT_COUNT (8)*/\
		    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)*/\
		    0xc0
#define HIDDESC_GAMEPAD_16B_SIZE 51

// Define workaround because we can't have conditionals in macros
#if MAX_AXIS == 1
#define HIDDESC_GPENTRY_32B_1 0x09, HID_USAGE_DESKTOP_X,             /*     USAGE (X)*/\
	HIDDESC_32B_ENTRY(0x01)
#else
#define HIDDESC_GPENTRY_32B_1 0x09, HID_USAGE_DESKTOP_X,             /*     USAGE (X)*/
#endif
#if MAX_AXIS == 2
#define HIDDESC_GPENTRY_32B_2 0x09, HID_USAGE_DESKTOP_Y,             /*     USAGE (Y)*/\
	HIDDESC_32B_ENTRY(0x02)
#else
#define HIDDESC_GPENTRY_32B_2 0x09, HID_USAGE_DESKTOP_Y,             /*     USAGE (Y)*/
#endif
#if MAX_AXIS == 3
#define HIDDESC_GPENTRY_32B_3 0x09, HID_USAGE_DESKTOP_Z,             /*     USAGE (Z)*/\
	HIDDESC_32B_ENTRY(0x02)
#else
#define HIDDESC_GPENTRY_32B_3 0x09, HID_USAGE_DESKTOP_Z,             /*     USAGE (Z)*/
#endif
#define HIDDESC_GAMEPAD_32B \
		    0xa1, 0x00,                    /*   COLLECTION (Physical)*/\
		    0x85, 0x01,                    /*     REPORT_ID (1)*/\
		    0x05, 0x09,                    /*     USAGE_PAGE (Button)*/\
		    0x19, 0x01,                    /*     USAGE_MINIMUM (Button 1)*/\
		    0x29, 0x40,                    /*     USAGE_MAXIMUM (Button 64)*/\
		    0x15, 0x00,                    /*     LOGICAL_MINIMUM (0)*/\
		    0x25, 0x01,                    /*     LOGICAL_MAXIMUM (1)*/\
		    0x95, 0x40,                    /*     REPORT_COUNT (64)*/\
		    0x75, 0x01,                    /*     REPORT_SIZE (1)*/\
		    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)*/\
		    0x05, 0x01,                    /*     USAGE_PAGE (Generic Desktop)*/\
			HIDDESC_GPENTRY_32B_1 \
			HIDDESC_GPENTRY_32B_2 \
			HIDDESC_GPENTRY_32B_3 \
		    0x09, HID_USAGE_DESKTOP_RX,            /*     USAGE (Rx)*/\
		    0x09, HID_USAGE_DESKTOP_RY,            /*     USAGE (Ry)*/\
			0x09, HID_USAGE_DESKTOP_RZ,            /*     USAGE (Rz)*/\
			0x09, HID_USAGE_DESKTOP_DIAL,           /*     USAGE (Dial)*/\
			0x09, HID_USAGE_DESKTOP_SLIDER,           /*     USAGE (Slider)*/\
		    0x16, 0x01, 0x80,              /*     LOGICAL_MINIMUM (-32767)*/\
		    0x26, 0xff, 0x7f,              /*     LOGICAL_MAXIMUM (32767)*/\
		    0x75, 0x10,                    /*     REPORT_SIZE (16)*/\
			0x95, 0x08-MAX_AXIS,           /*     REPORT_COUNT (8-32baxes)*/\
		    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)*/\
		    0xc0
#define HIDDESC_GAMEPAD_32B_SIZE HIDDESC_GAMEPAD_16B_SIZE + HIDDESC_32B_ENTRY_SIZE
// Control reports for HID command interface
#define HIDDESC_CTRL_REP(DIR) /* INPUT or OUTPUT*/\
	0x85,HID_ID_HIDCMD, /*    Report ID*/\
	0x09, 0x01,                    /*   USAGE (Vendor)*/\
	0x15, 0x00,                    /*   LOGICAL_MINIMUM (0)*/\
	0x26, 0x04,	0x00,			   /*   Logical Maximum 4*/\
	0x75, 0x08,                    /*   REPORT_SIZE (8)*/\
	0x95, 0x01,                    /*   REPORT_COUNT (1)*/\
	HID_##DIR(1),                    /*   INPUT (Data,Var,Abs)*/\
	\
	0x09, 0x02,                    /*   USAGE (Vendor) class address*/\
	0x75, 0x10,                    /*   REPORT_SIZE (16)*/\
	0x95, 0x01,                    /*   REPORT_COUNT (1)*/\
	HID_##DIR(1),                    /*   INPUT (Data,Var,Abs)*/\
	\
	0x09, 0x03,                    /*   USAGE (Vendor) class instance*/\
	0x75, 0x08,                    /*   REPORT_SIZE (8)*/\
	0x95, 0x01,                    /*   REPORT_COUNT (1)*/\
	HID_##DIR(1),                    /*   INPUT (Data,Var,Abs)*/\
	\
	0x09, 0x04,                    /*   USAGE (Vendor) cmd*/\
	0x75, 0x20,                    /*   REPORT_SIZE (32)*/\
	0x95, 0x01,                    /*   REPORT_COUNT (1)*/\
	HID_##DIR(1),                    /*   INPUT (Data,Var,Abs)*/\
	\
	0x09, 0x05,                    /*   USAGE (Vendor)*/\
	0x75, 0x40,                    /*   REPORT_SIZE (64) value*/\
	0x95, 0x01,                    /*   REPORT_COUNT (1)*/\
	HID_##DIR(1),                    /*   INPUT (Data,Var,Abs)*/\
	\
	0x09, 0x06,                    /*   USAGE (Vendor) address*/\
	0x75, 0x40,                    /*   REPORT_SIZE (64)*/\
	0x95, 0x01,                    /*   REPORT_COUNT (1)*/\
	HID_##DIR(1)                   /*   INPUT (Data,Var,Abs)*/
#define HIDDESC_CTRL_REPORTS \
		0x06, 0x00, 0xFF,              /* USAGE_PAGE (Vendor)*/\
		0x09, 0x00,                    /*   USAGE (Vendor)*/\
		0xA1, 0x01, 				   /* Collection (Application)*/\
		HIDDESC_CTRL_REP(OUTPUT),\
		HIDDESC_CTRL_REP(INPUT),\
	  0xc0                          /*   END_COLLECTION*/
#define HIDDESC_CTRL_REPORTS_SIZE 118

#define HIDDESC_FFB_STATEREP \
		0x05,0x0F, /*  Usage Page Physical Interface*/ \
		0x09,0x92,        /*    Usage PID State report*/\
		0xA1,0x02,        /*    Collection Datalink (logical)*/\
		   0x85,HID_ID_STATE+FFB_ID_OFFSET,    /*    Report ID 2*/\
/*			   0x09,0x22,        Usage Effect Block Index*/\
/*			   0x15,0x01,        Logical Minimum 1*/\
/*			   0x25,MAX_EFFECTS,        Logical Maximum 28h (40d)*/\
/*			   0x35,0x01,        Physical Minimum 1*/\
/*			   0x45,MAX_EFFECTS,        Physical Maximum 28h (40d)*/\
/*			   0x75,0x08,        Report Size 8*/\
/*			   0x95,0x01,        Report Count 1*/\
/*			   0x81,0x02,        Input (Variable)*/\
		   0x09,0x9F,    /*    Usage Device is Pause*/\
		   0x09,0xA0,    /*    Usage Actuators Enabled*/\
		   0x09,0xA4,    /*    Usage Safety Switch*/\
		   0x09,0xA6,    /*    Usage Actuator Power*/\
		   0x09,0x94,    /*    Usage Effect Playing*/\
	/*		   0x15,0x00,        Logical Minimum 0*/\
	/*		   0x25,0x01,        Logical Maximum 1*/\
	/*		   0x35,0x00,        Physical Minimum 0*/\
	/*		   0x45,0x01,        Physical Maximum 1*/\
	/*		   0x75,0x01,        Report Size 1*/\
	/*		   0x95,0x01,        Report Count 1*/\
	/*		   0x81,0x02,        Input (Variable)*/\
		   0x15,0x00,    /*    Logical Minimum 0*/\
		   0x25,0x01,    /*    Logical Maximum 1*/\
		   0x35,0x00,    /*    Physical Minimum 0*/\
		   0x45,0x01,    /*    Physical Maximum 1*/\
		   0x75,0x01,    /*    Report Size 1*/\
		   0x95,0x05,    /*    Report Count 4*/\
		   0x81,0x02,    /*    Input (Variable)*/\
		   0x95,0x03,    /*    Report Count 3*/\
		   0x81,0x03,    /*    Input (Constant, Variable)*/\
		0xC0        // End Collection
#define HIDDESC_FFB_STATEREP_SIZE 37

/*
Output
Collection  Datalink:
Usage Set Effect Report

ID:1
Effect Block Index:	8bit

subcollection Effect Type
12 effect types, 8bit each

*/
#define HIDDESC_FFB_SETEFREP \
		0x09,0x21,    /*    Usage Set Effect Report*/\
		0xA1,0x02,    /*    Collection Datalink (Logical)*/\
		   0x85,HID_ID_EFFREP+FFB_ID_OFFSET,    /*    Report ID 1*/\
		   0x09,0x22,    /*    Usage Effect Block Index*/\
		   0x15,0x01,    /*    Logical Minimum 1*/\
		   0x25,MAX_EFFECTS,    /*    Logical Maximum 28h (40d)*/\
		   0x35,0x01,    /*    Physical Minimum 1*/\
		   0x45,MAX_EFFECTS,    /*    Physical Maximum 28h (40d)*/\
		   0x75,0x08,    /*    Report Size 8*/\
		   0x95,0x01,    /*    Report Count 1*/\
		   0x91,0x02,    /*    Output (Variable)*/\
		   0x09,0x25,    /*    Usage Effect Type*/\
		   0xA1,0x02,    /*    Collection Datalink*/\
		 0x09, HID_USAGE_CONST,    /*    Usage ET Constant Force*/\
		 0x09, HID_USAGE_RAMP,    /*    Usage ET Ramp*/\
		 0x09, HID_USAGE_SQUR,    /*    Usage ET Square*/\
		 0x09, HID_USAGE_SINE,    /*    Usage ET Sine*/\
		 0x09, HID_USAGE_TRNG,    /*    Usage ET Triangle*/\
		 0x09, HID_USAGE_STUP,    /*    Usage ET Sawtooth Up*/\
		 0x09, HID_USAGE_STDN,    /*    Usage ET Sawtooth Down*/\
		 0x09, HID_USAGE_SPRNG,    /*    Usage ET Spring*/\
		 0x09, HID_USAGE_DMPR,    /*    Usage ET Damper*/\
		 0x09, HID_USAGE_INRT,    /*    Usage ET Inertia*/\
		 0x09, HID_USAGE_FRIC,    /*    Usage ET Friction*/\
		/* 0x09, 0x28,    //    Usage ET Custom Force Data*/\
		      0x25,0x0B,    /*    Logical Maximum Bh (11d)*/\
		      0x15,0x01,    /*    Logical Minimum 1*/\
		      0x35,0x01,    /*    Physical Minimum 1*/\
		      0x45,0x0B,    /*    Physical Maximum Bh (11d)*/\
		      0x75,0x08,    /*    Report Size 8*/\
		      0x95,0x01,    /*    Report Count 1*/\
		      0x91,0x00,    /*    Output*/\
		   0xC0    ,          /*    End Collection*/\
		   0x09,0x50,         /*    Usage Duration*/\
		   0x09,0x54,         /*    Usage Trigger Repeat Interval*/\
		   0x09,0x51,         /*    Usage Sample Period*/\
		   0x09,0xA7,         /*    Usage Start Delay*/\
		   0x15,0x00,         /*    Logical Minimum 0*/\
		   0x26,0xFF,0x7F,    /*    Logical Maximum 7FFFh (32767d)*/\
		   0x35,0x00,         /*    Physical Minimum 0*/\
		   0x46,0xFF,0x7F,    /*    Physical Maximum 7FFFh (32767d)*/\
		   0x66,0x03,0x10,    /*    Unit 1003h (4099d)*/\
		   0x55,0xFD,         /*    Unit Exponent FDh (253d)*/\
		   0x75,0x10,         /*    Report Size 10h (16d)*/\
		   0x95,0x04,         /*    Report Count 4*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		   0x55,0x00,         /*    Unit Exponent 0*/\
		   0x66,0x00,0x00,    /*    Unit 0*/\
		   0x09,0x52,         /*    Usage Gain*/\
		   0x15,0x00,         /*    Logical Minimum 0*/\
		   0x26,0xFF,0x00,    /*    Logical Maximum FFh (255d) // TODO scaling?*/\
		   0x35,0x00,         /*    Physical Minimum 0*/\
		   0x46,0x10,0x27,    /*    Physical Maximum 2710h (10000d)*/\
		   0x75,0x08,         /*    Report Size 8*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		   0x09,0x53,         /*    Usage Trigger Button*/\
		   0x15,0x01,         /*    Logical Minimum 1*/\
		   0x25,0x08,         /*    Logical Maximum 8*/\
		   0x35,0x01,         /*    Physical Minimum 1*/\
		   0x45,0x08,         /*    Physical Maximum 8*/\
		   0x75,0x08,         /*    Report Size 8*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02         //    Output (Variable)
#define HIDDESC_FFB_SETEFREP_SIZE 131

#define HIDDESC_FFB_SETENVREP \
		/* Envelope Report Definition*/\
		0x09,0x5A,    /*    Usage Set Envelope Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x85,HID_ID_ENVREP+FFB_ID_OFFSET,         /*    Report ID 2*/\
		   0x09,0x22,         /*    Usage Effect Block Index*/\
		   0x15,0x01,         /*    Logical Minimum 1*/\
		   0x25,MAX_EFFECTS,         /*    Logical Maximum 28h (40d)*/\
		   0x35,0x01,         /*    Physical Minimum 1*/\
		   0x45,MAX_EFFECTS,         /*    Physical Maximum 28h (40d)*/\
		   0x75,0x08,         /*    Report Size 8*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		   0x09,0x5B,         /*    Usage Attack Level*/\
		   0x09,0x5D,         /*    Usage Fade Level*/\
		 0x16,0x00,0x00,    /*    Logical Minimum 0*/\
		 0x26,0xFF,0x7F,    /*    Logical Maximum 7FFFh (32767d)*/\
		 0x36,0x00,0x00,    /*    Physical Minimum 0*/\
		 0x46,0xFF,0x7F,    /*    Physical Maximum 7FFFh (32767d)*/\
		 0x75,0x10,         /*    Report Size 16*/\
		 0x95,0x02,         /*    Report Count 2*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		 0x09, 0x5C,         /*    Usage Attack Time*/\
		 0x09, 0x5E,         /*    Usage Fade Time*/\
		 0x66, 0x03, 0x10,    /*    Unit 1003h (English Linear, Seconds)*/\
		 0x55, 0xFD,         /*    Unit Exponent FDh (X10^-3 ==> Milisecond)*/\
		 0x27, 0xFF, 0x7F, 0x00, 0x00,    /*    Logical Maximum FFFFFFFFh (4294967295)*/\
		 0x47, 0xFF, 0x7F, 0x00, 0x00,    /*    Physical Maximum FFFFFFFFh (4294967295)*/\
		 0x75, 0x20,         /*    Report Size 20h (32d)*/\
		 0x91, 0x02,         /*    Output (Variable)*/\
		 0x45, 0x00,         /*    Physical Maximum 0*/\
		   0x66,0x00,0x00,    /*    Unit 0*/\
		   0x55,0x00,         /*    Unit Exponent 0*/\
		0xC0                 /*    End Collection*/
#define HIDDESC_FFB_SETENVREP_SIZE 75

// Contains axis dependant parts. not used
#define HIDDESC_FFB_SETCONDREP \
		0x09,0x5F,    /*    Usage Set Condition Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x85,HID_ID_CONDREP+FFB_ID_OFFSET,    /*    Report ID 3*/\
		   0x09,0x22,    /*    Usage Effect Block Index*/\
		   0x15,0x01,    /*    Logical Minimum 1*/\
		   0x25,MAX_EFFECTS,    /*    Logical Maximum 28h (40d)*/\
		   0x35,0x01,    /*    Physical Minimum 1*/\
		   0x45,MAX_EFFECTS,    /*    Physical Maximum 28h (40d)*/\
		   0x75,0x08,    /*    Report Size 8*/\
		   0x95,0x01,    /*    Report Count 1*/\
		   0x91,0x02,    /*    Output (Variable)*/\
		   0x09,0x23,    /*    Usage Parameter Block Offset*/\
		   0x15,0x00,    /*    Logical Minimum 0*/\
		   0x25,0x03,    /*    Logical Maximum 3*/\
		   0x35,0x00,    /*    Physical Minimum 0*/\
		   0x45,0x03,    /*    Physical Maximum 3*/\
		   0x75,0x04,    /*    Report Size 4*/\
		   0x95,0x01,    /*    Report Count 1*/\
		   0x91,0x02,    /*    Output (Variable)*/\
		   0x09,0x58,    /*    Usage Type Specific Block Off...*/\
		   0xA1,0x02,    /*    Collection Datalink*/\
		      0x0B,0x01,0x00,0x0A,0x00,    /*    Usage Ordinals: Instance 1*/\
		      0x0B,0x02,0x00,0x0A,0x00,    /*    Usage Ordinals: Instance 2*/\
		      0x75,0x02,                   /*    Report Size 2*/\
		      0x95,0x02,                   /*    Report Count 2*/\
		      0x91,0x02,                   /*    Output (Variable)*/\
		   0xC0     ,         /*    End Collection*/\
		   0x16,0x00, 0x80,    /*    Logical Minimum 7FFFh (-32767d)*/\
		   0x26,0xff, 0x7f,    /*    Logical Maximum 7FFFh (32767d)*/\
		   0x36,0x00, 0x80,    /*    Physical Minimum 7FFFh (-32767d)*/\
		   0x46,0xff, 0x7f,    /*    Physical Maximum 7FFFh (32767d)*/\
\
		   0x09,0x60,         /*    Usage CP Offset*/\
		   0x75,0x10,         /*    Report Size 16*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		   0x36,0x00, 0x80,    /*    Physical Minimum  (-32768)*/\
		   0x46,0xff, 0x7f,    /*    Physical Maximum  (32767)*/\
		   0x09,0x61,         /*    Usage Positive Coefficient*/\
		   0x09,0x62,         /*    Usage Negative Coefficient*/\
		   0x95,0x02,         /*    Report Count 2*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		 0x16,0x00,0x00,    /*    Logical Minimum 0*/\
		 0x26,0xff, 0x7f,	  /*    Logical Maximum  (32767)*/\
		 0x36,0x00,0x00,    /*    Physical Minimum 0*/\
		   0x46,0xff, 0x7f,    /*    Physical Maximum  (32767)*/\
		   0x09,0x63,         /*    Usage Positive Saturation*/\
		   0x09,0x64,         /*    Usage Negative Saturation*/\
		   0x75,0x10,         /*    Report Size 16*/\
		   0x95,0x02,         /*    Report Count 2*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		   0x09,0x65,         /*    Usage Dead Band*/\
		   0x46,0xff, 0x7f,    /*    Physical Maximum (32767)*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		0xC0         /*    End Collection*/
#define HIDDESC_FFB_SETCONDREP_SIZE 125

#define HIDDESC_FFB_SETPERIODICREP \
		0x09,0x6E,    /*    Usage Set Periodic Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x85,HID_ID_PRIDREP+FFB_ID_OFFSET,                   /*    Report ID 4*/\
		   0x09,0x22,                   /*    Usage Effect Block Index*/\
		   0x15,0x01,                   /*    Logical Minimum 1*/\
		   0x25,MAX_EFFECTS,                   /*    Logical Maximum 28h (40d)*/\
		   0x35,0x01,                   /*    Physical Minimum 1*/\
		   0x45,MAX_EFFECTS,                   /*    Physical Maximum 28h (40d)*/\
		   0x75,0x08,                   /*    Report Size 8*/\
		   0x95,0x01,                   /*    Report Count 1*/\
		   0x91,0x02,                   /*    Output (Variable)*/\
		   0x09,0x70,                   /*    Usage Magnitude*/\
		 0x16,0x00,0x00,              /*    Logical Minimum 0*/\
		 0x26,0xff, 0x7f,    /*    Logical Maximum 7FFFh (32767d)*/\
		 0x36,0x00,0x00,              /*    Physical Minimum 0*/\
		 0x26,0xff, 0x7f,    /*    Logical Maximum 7FFFh (32767d)*/\
		   0x75,0x10,                   /*    Report Size 16*/\
		   0x95,0x01,                   /*    Report Count 1*/\
		   0x91,0x02,                   /*    Output (Variable)*/\
		 0x09, 0x6F,                   /*    Usage Offset*/\
		 0x16,0x00, 0x80,    /*    Logical Minimum 7FFFh (-32767d)*/\
	     0x26,0xff, 0x7f,    /*    Logical Maximum 7FFFh (32767d)*/\
	     0x36,0x00, 0x80,    /*    Physical Minimum 7FFFh (-32767d)*/\
	     0x46,0xff, 0x7f,    /*    Physical Maximum 7FFFh (32767d)*/\
		 0x95, 0x01,                   /*    Report Count 1*/\
		 0x75, 0x10,                   /*    Report Size 16*/\
		 0x91, 0x02,                   /*    Output (Variable)*/\
		 0x09, 0x71,                   /*    Usage Phase*/\
		 0x66, 0x14, 0x00,             /*    Unit 14h (Eng Rotation, Degrees)*/\
		 0x55, 0xFE,                   /*    Unit Exponent FEh (X10^-2)*/\
		 0x15, 0x00,                   /*    Logical Minimum 0*/\
		 0x27, 0x9F, 0x8C, 0x00, 0x00, /*    Logical Maximum 8C9Fh (35999d)*/\
		 0x35, 0x00,                   /*    Physical Minimum 0*/\
		 0x47, 0x9F, 0x8C, 0x00, 0x00, /*    Physical Maximum 8C9Fh (35999d)*/\
		 0x75, 0x10,                   /*    Report Size 16*/\
		 0x95, 0x01,                   /*    Report Count 1*/\
		 0x91, 0x02,                   /*    Output (Variable)*/\
		 0x09, 0x72,                   /*    Usage Period*/\
		 0x15, 0x01,                   /*    Logical Minimum 1*/\
		 0x27, 0xFF, 0x7F, 0x00, 0x00, /*    Logical Maximum 7FFFh (32K)*/\
		 0x35, 0x01,                   /*    Physical Minimum 1*/\
		 0x47, 0xFF, 0x7F, 0x00, 0x00, /*    Physical Maximum 7FFFh (32K)*/\
		 0x66, 0x03, 0x10,             /*    Unit 1003h (English Linear, Seconds)*/\
		 0x55, 0xFD,                   /*    Unit Exponent FDh (X10^-3 ==> Millisecond)*/\
		 0x75, 0x20,                   /*    Report Size 20h (32)*/\
		 0x95, 0x01,                   /*    Report Count 1*/\
		 0x91, 0x02,                   /*    Output (Variable)*/\
		 0x66, 0x00, 0x00,              /*    Unit 0*/\
		   0x55,0x00,                   /*    Unit Exponent 0*/\
		0xC0         /* End Collection*/
#define HIDDESC_FFB_SETPERIODICREP_SIZE 122

#define HIDDESC_FFB_SETCFREP \
		0x09,0x73,    /*    Usage Set Constant Force Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x85,HID_ID_CONSTREP+FFB_ID_OFFSET,         /*    Report ID 5*/\
		   0x09,0x22,         /*    Usage Effect Block Index*/\
		   0x15,0x01,         /*    Logical Minimum 1*/\
		   0x25,MAX_EFFECTS,         /*    Logical Maximum 28h (40d)*/\
		   0x35,0x01,         /*    Physical Minimum 1*/\
		   0x45,MAX_EFFECTS,         /*    Physical Maximum 28h (40d)*/\
		   0x75,0x08,         /*    Report Size 8*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		   0x09,0x70,         /*    Usage Magnitude*/\
		   0x16,0x01, 0x80,   /*    Logical Minimum -7FFFh (-32767d)*/\
		   0x26,0xff, 0x7f,    /*    Logical Maximum 7FFFh (32767d)*/\
		   0x36,0x01, 0x80,    /*    Physical Minimum -7FFFh (-32767d)*/\
		   0x46,0xff, 0x7f,    /*    Physical Maximum 7FFFh (32767d)*/\
		   0x75, 0x10,         /*    Report Size 10h (16d)*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		0xC0         /*    End Collection*/
#define HIDDESC_FFB_SETCFREP_SIZE 43

#define HIDDESC_FFB_SETRAMPREP \
		0x09,0x74,    /*    Usage Set Ramp Force Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x85,HID_ID_RAMPREP+FFB_ID_OFFSET,         /*    Report ID 6*/\
		   0x09,0x22,         /*    Usage Effect Block Index*/\
		   0x15,0x01,         /*    Logical Minimum 1*/\
		   0x25,MAX_EFFECTS,         /*    Logical Maximum 28h (40d)*/\
		   0x35,0x01,         /*    Physical Minimum 1*/\
		   0x45,MAX_EFFECTS,         /*    Physical Maximum 28h (40d)*/\
		   0x75,0x08,         /*    Report Size 8*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		   0x09,0x75,         /*    Usage Ramp Start*/\
		   0x09,0x76,         /*    Usage Ramp End*/\
		   0x16,0x00, 0x80,    /*    Logical Minimum 7FFFh (-32767d)*/\
		   0x26,0xff, 0x7f,    /*    Logical Maximum 7FFFh (32767d)*/\
		   0x36,0x00, 0x80,    /*    Physical Minimum 7FFFh (-32767d)*/\
		   0x46,0xff, 0x7f,    /*    Physical Maximum 7FFFh (32767d)*/\
		   0x75,0x10,         /*    Report Size 16*/\
		   0x95,0x02,         /*    Report Count 2*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		0xC0         /*    End Collection*/
#define HIDDESC_FFB_SETRAMPREP_SIZE 45
// not to be used yet
#define HIDDESC_FFB_CUSTOMFORCEREP \
/*			0x09,0x68,    //    Usage Custom Force Data Report*/\
/*			0xA1,0x02,    //    Collection Datalink*/\
/*			   0x85,HID_ID_CSTMREP+FFB_ID_OFFSET,         //    Report ID 7*/\
/*			   0x09,0x22,         //    Usage Effect Block Index*/\
/*			   0x15,0x01,         //    Logical Minimum 1*/\
/*			   0x25,MAX_EFFECTS,         //    Logical Maximum 28h (40d)*/\
/*			   0x35,0x01,         //    Physical Minimum 1*/\
/*			   0x45,MAX_EFFECTS,         //    Physical Maximum 28h (40d)*/\
/*			   0x75,0x08,         //    Report Size 8*/\
/*			   0x95,0x01,         //    Report Count 1*/\
/*			   0x91,0x02,         //    Output (Variable)*/\
/*			   0x09,0x6C,         //    Usage Custom Force Data Offset*/\
/*			   0x15,0x00,         //    Logical Minimum 0*/\
/*			   0x26,0x10,0x27,    //    Logical Maximum 2710h (10000d)*/\
/*			   0x35,0x00,         //    Physical Minimum 0*/\
/*			   0x46,0x10,0x27,    //    Physical Maximum 2710h (10000d)*/\
/*			   0x75,0x10,         //    Report Size 10h (16d)*/\
/*			   0x95,0x01,         //    Report Count 1*/\
/*			   0x91,0x02,         //    Output (Variable)*/\
/*			   0x09,0x69,         //    Usage Custom Force Data*/\
/*			   0x15,0x81,         //    Logical Minimum 81h (-127d)*/\
/*			   0x25,0x7F,         //    Logical Maximum 7Fh (127d)*/\
/*			   0x35,0x00,         //    Physical Minimum 0*/\
/*			   0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)*/\
/*			   0x75,0x08,         //    Report Size 8*/\
/*			   0x95,0x0C,         //    Report Count Ch (12d)*/\
/*			   0x92,0x02,0x01,    //       Output (Variable, Buffered)*/\
/*			0xC0     ,    //    End Collection*/\
/*			0x09,0x66,    //    Usage Download Force Sample*/\
/*			0xA1,0x02,    //    Collection Datalink*/\
/*			   0x85,HID_ID_SMPLREP+FFB_ID_OFFSET,         //    Report ID 8*/\
/*			   0x05,0x01,         //    Usage Page Generic Desktop*/\
/*			   0x09,0x30,         //    Usage X*/\
/*			   0x09,0x31,         //    Usage Y*/\
/*			   0x15,0x81,         //    Logical Minimum 81h (-127d)*/\
/*			   0x25,0x7F,         //    Logical Maximum 7Fh (127d)*/\
/*			   0x35,0x00,         //    Physical Minimum 0*/\
/*			   0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)*/\
/*			   0x75,0x08,         //    Report Size 8*/\
/*			   0x95,0x02,         //    Report Count 2*/\
/*			   0x91,0x02,         //    Output (Variable)*/\
/*			0xC0     ,   //    End Collection*/
#define HIDDESC_FFB_EFOPREP \
		0x05,0x0F,   /*    Usage Page Physical Interface*/\
		0x09,0x77,   /*    Usage Effect Operation Report*/\
		0xA1,0x02,   /*    Collection Datalink*/\
		   0x85,HID_ID_EFOPREP+FFB_ID_OFFSET,    /*    Report ID Ah (10d)*/\
		   0x09,0x22,    /*    Usage Effect Block Index*/\
		   0x15,0x01,    /*    Logical Minimum 1*/\
		   0x25,MAX_EFFECTS,    /*    Logical Maximum 28h (40d)*/\
		   0x35,0x01,    /*    Physical Minimum 1*/\
		   0x45,MAX_EFFECTS,    /*    Physical Maximum 28h (40d)*/\
		   0x75,0x08,    /*    Report Size 8*/\
		   0x95,0x01,    /*    Report Count 1*/\
		   0x91,0x02,    /*    Output (Variable)*/\
		   0x09,0x78,    /*    Usage Effect Operation*/\
		   0xA1,0x02,    /*    Collection Datalink*/\
		      0x09,0x79,    /*    Usage Op Effect Start*/\
		      0x09,0x7A,    /*    Usage Op Effect Start Solo*/\
		      0x09,0x7B,    /*    Usage Op Effect Stop*/\
		      0x15,0x01,    /*    Logical Minimum 1*/\
		      0x25,0x03,    /*    Logical Maximum 3*/\
		      0x75,0x08,    /*    Report Size 8*/\
		      0x95,0x01,    /*    Report Count 1*/\
		      0x91,0x00,    /*    Output*/\
		   0xC0     ,         /*    End Collection*/\
		   0x09,0x7C,         /*    Usage Loop Count*/\
		   0x15,0x00,         /*    Logical Minimum 0*/\
		   0x26,0xFF,0x00,    /*    Logical Maximum FFh (255d)*/\
		   0x35,0x00,         /*    Physical Minimum 0*/\
		   0x46,0xFF,0x00,    /*    Physical Maximum FFh (255d)*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		0xC0         /*    End Collection*/
#define HIDDESC_FFB_EFOPREP_SIZE 60
#define HIDDESC_FFB_BLOCKFREEREP \
		0x09,0x90,    /*    Usage PID Block Free Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x85,HID_ID_BLKFRREP+FFB_ID_OFFSET,    /*    Report ID Bh (11d)*/\
		   0x09,0x22,    /*    Usage Effect Block Index*/\
		   0x15,0x01,    /*    Logical Minimum 1*/\
		   0x25,MAX_EFFECTS,    /*    Logical Maximum 28h (40d)*/\
		   0x35,0x01,    /*    Physical Minimum 1*/\
		   0x45,MAX_EFFECTS,    /*    Physical Maximum 28h (40d)*/\
		   0x75,0x08,    /*    Report Size 8*/\
		   0x95,0x01,    /*    Report Count 1*/\
		   0x91,0x02,    /*    Output (Variable)*/\
		0xC0         /*    End Collection*/
#define HIDDESC_FFB_BLOCKFREEREP_SIZE 23
#define HIDDESC_FFB_DEVCTRLREP \
		0x09,0x95,    /*    Usage PID Device Control (0x96?)*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		0x85,HID_ID_CTRLREP+FFB_ID_OFFSET,    /*    Report ID Ch (12d)*/\
		0x09,0x96,    /*    Usage PID Device Control (0x96?)*/\
		0xA1,0x02,    /*    Collection Datalink*/\
\
		   0x09,0x97,    /*    Usage DC Enable Actuators*/\
		   0x09,0x98,    /*    Usage DC Disable Actuators*/\
		   0x09,0x99,    /*    Usage DC Stop All Effects*/\
		   0x09,0x9A,    /*    Usage DC Device Reset*/\
		   0x09,0x9B,    /*    Usage DC Device Pause*/\
		   0x09,0x9C,    /*    Usage DC Device Continue*/\
\
\
\
		   0x15,0x01,    /*    Logical Minimum 1*/\
		   0x25,0x06,    /*    Logical Maximum 6*/\
		   0x75,0x01,    /*    Report Size 1*/\
		   0x95,0x08,    /*    Report Count 8*/\
		   0x91,0x02,    /*    Output*/\
\
		0xC0     ,    /*    End Collection*/\
		0xC0     ,    /*    End Collection*/\
		0x09,0x7D,    /*    Usage Device Gain Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x85,HID_ID_GAINREP+FFB_ID_OFFSET,         /*    Report ID Dh (13d)*/\
		   0x09,0x7E,         /*    Usage Device Gain*/\
		   0x15,0x00,         /*    Logical Minimum 0*/\
		   0x26,0xFF,0x00,    /*    Logical Maximum FFh (255d)*/\
		   0x35,0x00,         /*    Physical Minimum 0*/\
		   0x46,0x10,0x27,    /*    Physical Maximum 2710h (10000d)*/\
		   0x75,0x08,         /*    Report Size 8*/\
		   0x95,0x01,         /*    Report Count 1*/\
		   0x91,0x02,         /*    Output (Variable)*/\
		0xC0                 /*    End Collection*/
#define HIDDESC_FFB_DEVCTRLREP_SIZE 59
// Do not use yet
#define HIDDESC_FFB_SETCUSTFORCEREP \
/*			0x09,0x6B,    //    Usage Set Custom Force Report*/\
/*			0xA1,0x02,    //    Collection Datalink*/\
/*			   0x85,HID_ID_SETCREP+FFB_ID_OFFSET,         //    Report ID Eh (14d)*/\
/*			   0x09,0x22,         //    Usage Effect Block Index*/\
/*			   0x15,0x01,         //    Logical Minimum 1*/\
/*			   0x25,MAX_EFFECTS,         //    Logical Maximum 28h (40d)*/\
/*			   0x35,0x01,         //    Physical Minimum 1*/\
/*			   0x45,MAX_EFFECTS,         //    Physical Maximum 28h (40d)*/\
/*			   0x75,0x08,         //    Report Size 8*/\
/*			   0x95,0x01,         //    Report Count 1*/\
/*			   0x91,0x02,         //    Output (Variable)*/\
/*			   0x09,0x6D,         //    Usage Sample Count*/\
/*			   0x15,0x00,         //    Logical Minimum 0*/\
/*			   0x26,0xFF,0x00,    //    Logical Maximum FFh (255d)*/\
/*			   0x35,0x00,         //    Physical Minimum 0*/\
/*			   0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)*/\
/*			   0x75,0x08,         //    Report Size 8*/\
/*			   0x95,0x01,         //    Report Count 1*/\
/*			   0x91,0x02,         //    Output (Variable)*/\
/*			   0x09,0x51,         //    Usage Sample Period*/\
/*			   0x66,0x03,0x10,    //    Unit 1003h (4099d)*/\
/*			   0x55,0xFD,         //    Unit Exponent FDh (253d)*/\
/*			   0x15,0x00,         //    Logical Minimum 0*/\
/*			   0x26,0xFF,0x7F,    //    Logical Maximum 7FFFh (32767d)*/\
/*			   0x35,0x00,         //    Physical Minimum 0*/\
/*			   0x46,0xFF,0x7F,    //    Physical Maximum 7FFFh (32767d)*/\
/*			   0x75,0x10,         //    Report Size 10h (16d)*/\
/*			   0x95,0x01,         //    Report Count 1*/\
/*			   0x91,0x02,         //    Output (Variable)*/\
/*			   0x55,0x00,         //    Unit Exponent 0*/\
/*			   0x66,0x00,0x00,    //    Unit 0*/\
/*			0xC0         //    End Collection*/
#define HIDDESC_FFB_NEWEFREP \
		0x09,0xAB,    /*    Usage Create New Effect Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x85,HID_ID_NEWEFREP+FFB_ID_OFFSET,    /*    Report ID 1*/\
		   0x09,0x25,    /*    Usage Effect Type*/\
		   0xA1,0x02,    /*    Collection Datalink*/\
		 0x09, HID_USAGE_CONST,    /*    Usage ET Constant Force*/\
		 0x09, HID_USAGE_RAMP,    /*    Usage ET Ramp*/\
		 0x09, HID_USAGE_SQUR,    /*    Usage ET Square*/\
		 0x09, HID_USAGE_SINE,    /*    Usage ET Sine*/\
		 0x09, HID_USAGE_TRNG,    /*    Usage ET Triangle*/\
		 0x09, HID_USAGE_STUP,    /*    Usage ET Sawtooth Up*/\
		 0x09, HID_USAGE_STDN,    /*    Usage ET Sawtooth Down*/\
		 0x09, HID_USAGE_SPRNG,    /*    Usage ET Spring*/\
		 0x09, HID_USAGE_DMPR,    /*    Usage ET Damper*/\
		 0x09, HID_USAGE_INRT,    /*    Usage ET Inertia*/\
		 0x09, HID_USAGE_FRIC,    /*    Usage ET Friction*/\
/*			 0x09, 0x28,    //    Usage ET Custom Force Data*/\
		   0x25,0x0B,    /*    Logical Maximum Ch (11d)*/\
		   0x15,0x01,    /*    Logical Minimum 1*/\
		   0x35,0x01,    /*    Physical Minimum 1*/\
		   0x45,0x0B,    /*    Physical Maximum Ch (11d)*/\
		   0x75,0x08,    /*    Report Size 8*/\
		   0x95,0x01,    /*    Report Count 1*/\
		   0xB1,0x00,    /*    Feature*/\
		0xC0     ,    /* End Collection*/\
		0x05,0x01,         /*    Usage Page Generic Desktop*/\
		0x09,0x3B,         /*    Usage Reserved (Byte count)*/\
		0x15,0x00,         /*    Logical Minimum 0*/\
		0x26,0xFF,0x01,    /*    Logical Maximum 1FFh (511d)*/\
		0x35,0x00,         /*    Physical Minimum 0*/\
		0x46,0xFF,0x01,    /*    Physical Maximum 1FFh (511d)*/\
		0x75,0x0A,         /*    Report Size Ah (10d)*/\
		0x95,0x01,         /*    Report Count 1*/\
		0xB1,0x02,         /*    Feature (Variable)*/\
		0x75,0x06,         /*    Report Size 6*/\
		0xB1,0x01,         /*    Feature (Constant)*/\
		0xC0         /*    End Collection*/
#define HIDDESC_FFB_NEWEFREP_SIZE 72

#define HIDDESC_FFB_BLOCKLOADREP \
		0x05,0x0F,    /*    Usage Page Physical Interface*/\
		0x09,0x89,    /*    Usage Block Load Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		0x85,HID_ID_BLKLDREP+FFB_ID_OFFSET,    /*    Report ID 0x12*/\
		0x09,0x22,    /*    Usage Effect Block Index*/\
		0x25,MAX_EFFECTS,    /*    Logical Maximum 28h (40d)*/\
		0x15,0x01,    /*    Logical Minimum 1*/\
		0x35,0x01,    /*    Physical Minimum 1*/\
		0x45,MAX_EFFECTS,    /*    Physical Maximum 28h (40d)*/\
		0x75,0x08,    /*    Report Size 8*/\
		0x95,0x01,    /*    Report Count 1*/\
		0xB1,0x02,    /*    Feature (Variable)*/\
		0x09,0x8B,    /*    Usage Block Load Status*/\
		0xA1,0x02,    /*    Collection Datalink*/\
		   0x09,0x8C,    /*    Usage Block Load Success*/\
		   0x09,0x8D,    /*    Usage Block Load Full*/\
		   0x09,0x8E,    /*    Usage Block Load Error*/\
		   0x15,0x01,    /*    Logical Minimum 1*/\
		   0x25,0x03,    /*    Logical Maximum 3*/\
		   0x35,0x01,    /*    Physical Minimum 1*/\
		   0x45,0x03,    /*    Physical Maximum 3*/\
		   0x75,0x08,    /*    Report Size 8*/\
		   0x95,0x01,    /*    Report Count 1*/\
		   0xB1,0x00,    /*    Feature*/\
		0xC0     ,                   /* End Collection*/\
		0x09,0xAC,                   /*    Usage Pool available*/\
		0x15,0x00,                   /*    Logical Minimum 0*/\
		0x27,0xFF,0xFF,0x00,0x00,    /*    Logical Maximum FFFFh (65535d)*/\
		0x35,0x00,                   /*    Physical Minimum 0*/\
		0x47,0xFF,0xFF,0x00,0x00,    /*    Physical Maximum FFFFh (65535d)*/\
		0x75,0x10,                   /*    Report Size 10h (16d)*/\
		0x95,0x01,                   /*    Report Count 1*/\
		0xB1,0x00,                   /*    Feature*/\
		0xC0         //    End Collection
#define HIDDESC_FFB_BLOCKLOADREP_SIZE 72

#define HIDDESC_FFB_POOLREP \
		0x09,0x7F,    /*    Usage PID Pool Report*/\
		0xA1,0x02,    /*    Collection Datalink*/\
			0x85,HID_ID_POOLREP+FFB_ID_OFFSET,                   /*    Report ID 0x13*/\
			0x09,0x80,                   /*    Usage RAM Pool size*/\
			0x75,0x10,                   /*    Report Size 10h (16d)*/\
			0x95,0x01,                   /*    Report Count 1*/\
			0x15,0x00,                   /*    Logical Minimum 0*/\
			0x35,0x00,                   /*    Physical Minimum 0*/\
			0x27,0xFF,0xFF,0x00,0x00,    /*    Logical Maximum FFFFh (65535d)*/\
			0x47,0xFF,0xFF,0x00,0x00,    /*    Physical Maximum FFFFh (65535d)*/\
			0xB1,0x02,                   /*    Feature (Variable)*/\
			0x09,0x83,                   /*    Usage Simultaneous Effects Max*/\
			0x26,0xFF,0x00,              /*    Logical Maximum FFh (255d)*/\
			0x46,0xFF,0x00,              /*    Physical Maximum FFh (255d)*/\
			0x75,0x08,                   /*    Report Size 8*/\
			0x95,0x01,                   /*    Report Count 1*/\
			0xB1,0x02,                   /*    Feature (Variable)*/\
			0x09,0xA9,                   /*    Usage Device Managed Pool*/\
			0x09,0xAA,                   /*    Usage Shared Parameter Blocks*/\
			0x75,0x01,                   /*    Report Size 1*/\
			0x95,0x02,                   /*    Report Count 2*/\
			0x15,0x00,                   /*    Logical Minimum 0*/\
			0x25,0x01,                   /*    Logical Maximum 1*/\
			0x35,0x00,                   /*    Physical Minimum 0*/\
			0x45,0x01,                   /*    Physical Maximum 1*/\
			0xB1,0x02,                   /*    Feature (Variable)*/\
			0x75,0x06,                   /*    Report Size 6*/\
			0x95,0x01,                   /*    Report Count 1*/\
			0xB1,0x03,                   /*    Feature (Constant, Variable)*/\
		0xC0 //    End Collection
#define HIDDESC_FFB_POOLREP_SIZE 67

#define HIDDESC_FFB_DESCSIZE HIDDESC_FFB_STATEREP_SIZE + HIDDESC_FFB_POOLREP_SIZE + HIDDESC_FFB_BLOCKLOADREP_SIZE + HIDDESC_FFB_NEWEFREP_SIZE + HIDDESC_FFB_DEVCTRLREP_SIZE + HIDDESC_FFB_BLOCKFREEREP_SIZE + HIDDESC_FFB_EFOPREP_SIZE + HIDDESC_FFB_SETEFREP_SIZE + HIDDESC_FFB_SETRAMPREP_SIZE + HIDDESC_FFB_SETCFREP_SIZE + HIDDESC_FFB_SETPERIODICREP_SIZE + HIDDESC_FFB_SETENVREP_SIZE



#ifdef AXIS2_FFB_HID_DESC
#define USB_HID_2FFB_REPORT_DESC_SIZE HIDDESC_GAMEPAD_16B_SIZE + HIDDESC_CTRL_REPORTS_SIZE + HIDDESC_FFB_DESCSIZE + 7 + 108 + 125
extern const uint8_t hid_2ffb_desc[USB_HID_2FFB_REPORT_DESC_SIZE];
#endif
#ifdef AXIS2_FFB_HID_DESC_32B
#define USB_HID_2FFB_REPORT_DESC_32B_SIZE HIDDESC_GAMEPAD_32B_SIZE + HIDDESC_CTRL_REPORTS_SIZE + HIDDESC_FFB_DESCSIZE + 7 + 108 + 125
extern const uint8_t hid_2ffb_desc_32b[USB_HID_2FFB_REPORT_DESC_32B_SIZE];
#endif

#ifdef AXIS1_FFB_HID_DESC
#define USB_HID_1FFB_REPORT_DESC_SIZE HIDDESC_GAMEPAD_16B_SIZE + HIDDESC_CTRL_REPORTS_SIZE + HIDDESC_FFB_DESCSIZE + 7 + 94 + 120
extern const uint8_t hid_1ffb_desc[USB_HID_1FFB_REPORT_DESC_SIZE];
#endif
#ifdef AXIS1_FFB_HID_DESC_32B
#define USB_HID_1FFB_REPORT_DESC_SIZE HIDDESC_GAMEPAD_32B_SIZE + HIDDESC_CTRL_REPORTS_SIZE + HIDDESC_FFB_DESCSIZE + 7 + 94 + 120
extern const uint8_t hid_1ffb_desc_32b[USB_HID_1FFB_REPORT_DESC_32B_SIZE];
#endif


#ifdef FFB_HID_DESC_GAMEPAD
#define USB_HID_GAMEPAD_REPORT_DESC_SIZE HIDDESC_GAMEPAD_16B_SIZE + HIDDESC_CTRL_REPORTS_SIZE + 7
extern const uint8_t hid_gamepad_desc[USB_HID_GAMEPAD_REPORT_DESC_SIZE];
#endif
#ifdef FFB_HID_DESC_GAMEPAD_32B
#define USB_HID_GAMEPAD_REPORT_DESC_32B_SIZE HIDDESC_GAMEPAD_32B_SIZE + HIDDESC_CTRL_REPORTS_SIZE + 7
extern const uint8_t hid_gamepad_desc_32b[USB_HID_GAMEPAD_REPORT_DESC_32B_SIZE];
#endif

/** @}*/
#endif /* USB_INC_USB_HID_FFB_DESC_H_ */
