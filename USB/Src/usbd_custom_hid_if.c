/**
  ******************************************************************************
  * @file           : usbd_custom_hid_if.c
  * @version        : v1.0_Cube
  * @brief          : USB Device Custom HID interface file.
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2019 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <global_callbacks.h>
#include "usbd_custom_hid_if.h"

/* USER CODE BEGIN INCLUDE */
#include "cppmain.h"
#include "ffb_defs.h"
#include "constants.h"
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device.
  * @{
  */

/** @addtogroup USBD_CUSTOM_HID
  * @{
  */

/** @defgroup USBD_CUSTOM_HID_Private_TypesDefinitions USBD_CUSTOM_HID_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Private_Defines USBD_CUSTOM_HID_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */

/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Private_Macros USBD_CUSTOM_HID_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Private_Variables USBD_CUSTOM_HID_Private_Variables
  * @brief Private variables.
  * @{
  */

/** Usb HID report descriptor. */
__ALIGN_BEGIN static uint8_t CUSTOM_HID_ReportDesc_FS[USBD_CUSTOM_HID_REPORT_DESC_SIZE] __ALIGN_END =
{
  /* USER CODE BEGIN 0 */
		   0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
		    0x09, 0x05,                    // USAGE (Joystick)
		    0xa1, 0x01,                    // COLLECTION (Application)
		    0xa1, 0x00,                    //   COLLECTION (Physical)
		    0x85, 0x01,                    //     REPORT_ID (1)
		    0x05, 0x09,                    //     USAGE_PAGE (Button)
		    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
		    0x29, 0x10,                    //     USAGE_MAXIMUM (Button 16)
		    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
		    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
		    0x95, 0x10,                    //     REPORT_COUNT (16)
		    0x75, 0x01,                    //     REPORT_SIZE (1)
		    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
		    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
		    0x09, 0x30,                    //     USAGE (X)
		    0x09, 0x31,                    //     USAGE (Y)
		    0x09, 0x32,                    //     USAGE (Z)
		    0x09, 0x33,                    //     USAGE (Rx)
		    0x09, 0x34,                    //     USAGE (Ry)
			0x09, 0x35,                    //     USAGE (Rz)
			0x09, 0x36,                    //     USAGE (Slider)
		    0x16, 0x00, 0x80,              //     LOGICAL_MINIMUM (-32768)
		    0x26, 0xff, 0x7f,              //     LOGICAL_MAXIMUM (32767)
		    0x75, 0x10,                    //     REPORT_SIZE (16)
		    0x95, 0x07,                    //     REPORT_COUNT (7)
		    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
		    0xc0,                          //   END_COLLECTION


			// Feature report (12)
			/*
			0x09, 0x00,                    //   USAGE (Reserved (no event indicated))
			0x75, 0x08,                    //   REPORT_SIZE (8)
			0x95, 0x01,                    //   REPORT_COUNT (1)
			0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
			0x25, 0xff,                    //   LOGICAL_MAXIMUM (255)
			0xb1, 0x02,                    //   FEATURE (Data,Var,Abs)
*/


		// BEGIN PID effects
			0x05,0x0F,        //    Usage Page Physical Interface
			0x09,0x92,        //    Usage PID State report
			0xA1,0x02,        //    Collection Datalink (logical)
			   0x85,HID_ID_STATE+FFB_ID_OFFSET,    //    Report ID 2

			   0x09,0x22,    //    Usage Effect Block Index
			   0x15,0x01,    //    Logical Minimum 1
			   0x25,MAX_EFFECTS,    //    Logical Maximum 28h (40d)
			   0x35,0x01,    //    Physical Minimum 1
			   0x45,MAX_EFFECTS,    //    Physical Maximum 28h (40d)
			   0x75,0x08,    //    Report Size 8
			   0x95,0x01,    //    Report Count 1
			   0x81,0x02,    //    Input (Variable)



			   0x09,0x9F,    //    Usage Device is Pause
			   0x09,0xA0,    //    Usage Actuators Enabled
			   0x09,0xA4,    //    Usage Safety Switch
			   0x09,0xA6,    //    Usage Actuator Power

			   0x09,0x94,    //    Usage Effect Playing
			   			   /*
			   			   			   0x15,0x00,    //    Logical Minimum 0
			   			   			   0x25,0x01,    //    Logical Maximum 1
			   			   			   0x35,0x00,    //    Physical Minimum 0
			   			   			   0x45,0x01,    //    Physical Maximum 1
			   			   			   0x75,0x01,    //    Report Size 1
			   			   			   0x95,0x01,    //    Report Count 1
			   			   			   0x81,0x02,    //    Input (Variable)*/ //14

			   0x15,0x00,    //    Logical Minimum 0
			   0x25,0x01,    //    Logical Maximum 1
			   0x35,0x00,    //    Physical Minimum 0
			   0x45,0x01,    //    Physical Maximum 1
			   0x75,0x01,    //    Report Size 1
			   0x95,0x05,    //    Report Count 4
			   0x81,0x02,    //    Input (Variable)
			   0x95,0x03,    //    Report Count 3
			   0x81,0x03,    //    Input (Constant, Variable)
			0xC0    ,    // End Collection

			/*
			Output
			Collection  Datalink:
			Usage Set Effect Report

			ID:1
			Effect Block Index:	8bit

			subcollection Effect Type
			12 effect types, 8bit each

			*/
			0x09,0x21,    //    Usage Set Effect Report
			0xA1,0x02,    //    Collection Datalink (Logical)
			   0x85,HID_ID_EFFREP+FFB_ID_OFFSET,    //    Report ID 1
			   0x09,0x22,    //    Usage Effect Block Index
			   0x15,0x01,    //    Logical Minimum 1
			   0x25,MAX_EFFECTS,    //    Logical Maximum 28h (40d)
			   0x35,0x01,    //    Physical Minimum 1
			   0x45,MAX_EFFECTS,    //    Physical Maximum 28h (40d)
			   0x75,0x08,    //    Report Size 8
			   0x95,0x01,    //    Report Count 1
			   0x91,0x02,    //    Output (Variable)
			   0x09,0x25,    //    Usage Effect Type
			   0xA1,0x02,    //    Collection Datalink
			 0x09, HID_USAGE_CONST,    //    Usage ET Constant Force
			 0x09, HID_USAGE_RAMP,    //    Usage ET Ramp
			 0x09, HID_USAGE_SQUR,    //    Usage ET Square
			 0x09, HID_USAGE_SINE,    //    Usage ET Sine
			 0x09, HID_USAGE_TRNG,    //    Usage ET Triangle
			 0x09, HID_USAGE_STUP,    //    Usage ET Sawtooth Up
			 0x09, HID_USAGE_STDN,    //    Usage ET Sawtooth Down
			 0x09, HID_USAGE_SPRNG,    //    Usage ET Spring
			 0x09, HID_USAGE_DMPR,    //    Usage ET Damper
			 0x09, HID_USAGE_INRT,    //    Usage ET Inertia
			 0x09, HID_USAGE_FRIC,    //    Usage ET Friction
			 0x09, 0x28,    //    Usage ET Custom Force Data
			      0x25,0x0C,    //    Logical Maximum Ch (12d)
			      0x15,0x01,    //    Logical Minimum 1
			      0x35,0x01,    //    Physical Minimum 1
			      0x45,0x0C,    //    Physical Maximum Ch (12d)
			      0x75,0x08,    //    Report Size 8
			      0x95,0x01,    //    Report Count 1
			      0x91,0x00,    //    Output
			   0xC0    ,          //    End Collection
			   0x09,0x50,         //    Usage Duration
			   0x09,0x54,         //    Usage Trigger Repeat Interval
			   0x09,0x51,         //    Usage Sample Period
			   0x15,0x00,         //    Logical Minimum 0
			   0x26,0xFF,0x7F,    //    Logical Maximum 7FFFh (32767d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0xFF,0x7F,    //    Physical Maximum 7FFFh (32767d)
			   0x66,0x03,0x10,    //    Unit 1003h (4099d)
			   0x55,0xFD,         //    Unit Exponent FDh (253d)
			   0x75,0x10,         //    Report Size 10h (16d)
			   0x95,0x03,         //    Report Count 3
			   0x91,0x02,         //    Output (Variable)
			   0x55,0x00,         //    Unit Exponent 0
			   0x66,0x00,0x00,    //    Unit 0
			   0x09,0x52,         //    Usage Gain
			   0x15,0x00,         //    Logical Minimum 0
			   0x26,0xFF,0x00,    //    Logical Maximum FFh (255d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0x10,0x27,    //    Physical Maximum 2710h (10000d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x53,         //    Usage Trigger Button
			   0x15,0x01,         //    Logical Minimum 1
			   0x25,0x08,         //    Logical Maximum 8
			   0x35,0x01,         //    Physical Minimum 1
			   0x45,0x08,         //    Physical Maximum 8
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)

			   0x09,0x55,       //    Usage Axes Enable TODO
			   0xA1,0x02,       //    Collection Datalink
			      0x05,0x01,    //    Usage Page Generic Desktop
			      0x09,0x30,    //    Usage X
			      0x09,0x31,    //    Usage Y
			      0x15,0x00,    //    Logical Minimum 0
			      0x25,0x01,    //    Logical Maximum 1
			      0x75,0x01,    //    Report Size 1
			      0x95,0x02,    //    Report Count 2
			      0x91,0x02,    //    Output (Variable)
			   0xC0     ,    // End Collection

			   0x05,0x0F,    //    Usage Page Physical Interface
			   0x09,0x56,    //    Usage Direction Enable
			   0x95,0x01,    //    Report Count 1
			   0x91,0x02,    //    Output (Variable)
			   0x95,0x05,    //    Report Count 5
			   0x91,0x03,    //    Output (Constant, Variable)

			   0x09,0x57,    //    Usage Direction
			   0xA1,0x02,    //    Collection Datalink
			      0x0B,0x01,0x00,0x0A,0x00,    //    Usage Ordinals: Instance 1
			      0x0B,0x02,0x00,0x0A,0x00,    //    Usage Ordinals: Instance 2
			      0x66,0x14,0x00,              //    Unit 14h (20d)
			      0x55,0xFE,                   //    Unit Exponent FEh (254d)
			      0x15,0x00,                   //    Logical Minimum 0
			      0x26,0xFF,0x00,              //    Logical Maximum FFh (255d)
			      0x35,0x00,                   //    Physical Minimum 0
			      0x47,0xA0,0x8C,0x00,0x00,    //    Physical Maximum 8CA0h (36000d)
			      0x66,0x00,0x00,              //    Unit 0
			      0x75,0x08,                   //    Report Size 8
			      0x95,0x02,                   //    Report Count 2
			      0x91,0x02,                   //    Output (Variable)
			      0x55,0x00,                   //    Unit Exponent 0
			      0x66,0x00,0x00,              //    Unit 0
			   0xC0,                           //    End Collection

			   0x05, 0x0F,        //     USAGE_PAGE (Physical Interface)
			   0x09, 0x58,        //     USAGE (Type Specific Block Offset)
			   0xA1, 0x02,        //     COLLECTION (Logical)
			      0x0B, 0x01, 0x00, 0x0A, 0x00, //USAGE (Ordinals:Instance 1
			      0x0B, 0x02, 0x00, 0x0A, 0x00, //USAGE (Ordinals:Instance 2)
			      0x26, 0xFD, 0x7F, //   LOGICAL_MAXIMUM (32765) ; 32K RAM or ROM max.
			      0x75, 0x10,     //     REPORT_SIZE (16)
			      0x95, 0x02,     //     REPORT_COUNT (2)
			      0x91, 0x02,     //     OUTPUT (Data,Var,Abs)
			   0xC0,              //     END_COLLECTION
			0xC0,                 //     END_COLLECTION

			// Envelope Report Definition
			0x09,0x5A,    //    Usage Set Envelope Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_ENVREP+FFB_ID_OFFSET,         //    Report ID 2
			   0x09,0x22,         //    Usage Effect Block Index
			   0x15,0x01,         //    Logical Minimum 1
			   0x25,MAX_EFFECTS,         //    Logical Maximum 28h (40d)
			   0x35,0x01,         //    Physical Minimum 1
			   0x45,MAX_EFFECTS,         //    Physical Maximum 28h (40d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x5B,         //    Usage Attack Level
			   0x09,0x5D,         //    Usage Fade Level
			 0x16,0x00,0x00,    //    Logical Minimum 0
			 0x26,0x10,0x27,    //    Logical Maximum 2710h (10000d)
			 0x36,0x00,0x00,    //    Physical Minimum 0
			   0x46,0x10,0x27,    //    Physical Maximum 2710h (10000d)
			 0x75,0x10,         //    Report Size 16
			 0x95,0x02,         //    Report Count 2
			   0x91,0x02,         //    Output (Variable)
			 0x09, 0x5C,         //    Usage Attack Time
			 0x09, 0x5E,         //    Usage Fade Time
			 0x66, 0x03, 0x10,    //    Unit 1003h (English Linear, Seconds)
			 0x55, 0xFD,         //    Unit Exponent FDh (X10^-3 ==> Milisecond)
			 0x27, 0xFF, 0x7F, 0x00, 0x00,    //    Logical Maximum FFFFFFFFh (4294967295)
			 0x47, 0xFF, 0x7F, 0x00, 0x00,    //    Physical Maximum FFFFFFFFh (4294967295)
			 0x75, 0x20,         //    Report Size 20h (32d)
			 0x91, 0x02,         //    Output (Variable)
			 0x45, 0x00,         //    Physical Maximum 0
			   0x66,0x00,0x00,    //    Unit 0
			   0x55,0x00,         //    Unit Exponent 0
			0xC0     ,            //    End Collection
			0x09,0x5F,    //    Usage Set Condition Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_CONDREP+FFB_ID_OFFSET,    //    Report ID 3
			   0x09,0x22,    //    Usage Effect Block Index
			   0x15,0x01,    //    Logical Minimum 1
			   0x25,MAX_EFFECTS,    //    Logical Maximum 28h (40d)
			   0x35,0x01,    //    Physical Minimum 1
			   0x45,MAX_EFFECTS,    //    Physical Maximum 28h (40d)
			   0x75,0x08,    //    Report Size 8
			   0x95,0x01,    //    Report Count 1
			   0x91,0x02,    //    Output (Variable)
			   0x09,0x23,    //    Usage Parameter Block Offset
			   0x15,0x00,    //    Logical Minimum 0
			   0x25,0x03,    //    Logical Maximum 3
			   0x35,0x00,    //    Physical Minimum 0
			   0x45,0x03,    //    Physical Maximum 3
			   0x75,0x04,    //    Report Size 4
			   0x95,0x01,    //    Report Count 1
			   0x91,0x02,    //    Output (Variable)
			   0x09,0x58,    //    Usage Type Specific Block Off...
			   0xA1,0x02,    //    Collection Datalink
			      0x0B,0x01,0x00,0x0A,0x00,    //    Usage Ordinals: Instance 1
			      0x0B,0x02,0x00,0x0A,0x00,    //    Usage Ordinals: Instance 2
			      0x75,0x02,                   //    Report Size 2
			      0x95,0x02,                   //    Report Count 2
			      0x91,0x02,                   //    Output (Variable)
			   0xC0     ,         //    End Collection
			 0x16,0x00, 0x80,    //    Logical Minimum 7FFFh (-32767d)
			 0x26,0xff, 0x7f,    //    Logical Maximum 7FFFh (32767d)
			   0x36,0x00, 0x80,    //    Physical Minimum 7FFFh (-32767d)
			   0x46,0xff, 0x7f,    //    Physical Maximum 7FFFh (32767d)

			   0x09,0x60,         //    Usage CP Offset
			   0x75,0x10,         //    Report Size 16
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x36,0x00, 0x80,    //    Physical Minimum  (-32768)
			   0x46,0xff, 0x7f,    //    Physical Maximum  (32767)
			   0x09,0x61,         //    Usage Positive Coefficient
			   0x09,0x62,         //    Usage Negative Coefficient
			   0x95,0x02,         //    Report Count 2
			   0x91,0x02,         //    Output (Variable)
			 0x16,0x00,0x00,    //    Logical Minimum 0
			 0x26,0xff, 0x7f,	  //    Logical Maximum  (32767)
			 0x36,0x00,0x00,    //    Physical Minimum 0
			   0x46,0xff, 0x7f,    //    Physical Maximum  (32767)
			   0x09,0x63,         //    Usage Positive Saturation
			   0x09,0x64,         //    Usage Negative Saturation
			   0x75,0x10,         //    Report Size 16
			   0x95,0x02,         //    Report Count 2
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x65,         //    Usage Dead Band
			   0x46,0xff, 0x7f,    //    Physical Maximum (32767)
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			0xC0     ,    //    End Collection
			0x09,0x6E,    //    Usage Set Periodic Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_PRIDREP+FFB_ID_OFFSET,                   //    Report ID 4
			   0x09,0x22,                   //    Usage Effect Block Index
			   0x15,0x01,                   //    Logical Minimum 1
			   0x25,MAX_EFFECTS,                   //    Logical Maximum 28h (40d)
			   0x35,0x01,                   //    Physical Minimum 1
			   0x45,MAX_EFFECTS,                   //    Physical Maximum 28h (40d)
			   0x75,0x08,                   //    Report Size 8
			   0x95,0x01,                   //    Report Count 1
			   0x91,0x02,                   //    Output (Variable)
			   0x09,0x70,                   //    Usage Magnitude
			 0x16,0x00,0x00,              //    Logical Minimum 0
			 0x26,0xff, 0x7f,    //    Logical Maximum 7FFFh (32767d)
			 0x36,0x00,0x00,              //    Physical Minimum 0
			 0x26,0xff, 0x7f,    //    Logical Maximum 7FFFh (32767d)
			   0x75,0x10,                   //    Report Size 16
			   0x95,0x01,                   //    Report Count 1
			   0x91,0x02,                   //    Output (Variable)
			 0x09, 0x6F,                   //    Usage Offset
			 0x16, 0xF0, 0xD8,             //    Logical Minimum D8F0h (-10000d)
			 0x26, 0x10, 0x27,             //    Logical Maximum D8F0h (-10000d)
			 0x36, 0xF0, 0xD8,              //    Physical Minimum D8F0h (-10000d)
			 0x46, 0x10, 0x27,              //    Physical Maximum D8F0h (-10000d)
			 0x95, 0x01,                   //    Report Count 1
			 0x75, 0x10,                   //    Report Size 16
			 0x91, 0x02,                   //    Output (Variable)
			 0x09, 0x71,                   //    Usage Phase
			 0x66, 0x14, 0x00,             //    Unit 14h (Eng Rotation, Degrees)
			 0x55, 0xFE,                   //    Unit Exponent FEh (X10^-2)
			 0x15, 0x00,                   //    Logical Minimum 0
			 0x27, 0x9F, 0x8C, 0x00, 0x00, //    Logical Maximum 8C9Fh (35999d)
			 0x35, 0x00,                   //    Physical Minimum 0
			 0x47, 0x9F, 0x8C, 0x00, 0x00, //    Physical Maximum 8C9Fh (35999d)
			 0x75, 0x10,                   //    Report Size 16
			 0x95, 0x01,                   //    Report Count 1
			 0x91, 0x02,                   //    Output (Variable)
			 0x09, 0x72,                   //    Usage Period
			 0x15, 0x00,                   //    Logical Minimum 0
			 0x27, 0xFF, 0x7F, 0x00, 0x00, //    Logical Maximum 7FFFh (32K)
			 0x35, 0x00,                   //    Physical Minimum 0
			 0x47, 0xFF, 0x7F, 0x00, 0x00, //    Physical Maximum 7FFFh (32K)
			 0x66, 0x03, 0x10,             //    Unit 1003h (English Linear, Seconds)
			 0x55, 0xFD,                   //    Unit Exponent FDh (X10^-3 ==> Milisecond)
			 0x75, 0x20,                   //    Report Size 20h (32)
			 0x95, 0x01,                   //    Report Count 1
			 0x91, 0x02,                   //    Output (Variable)
			 0x66, 0x00, 0x00,              //    Unit 0
			   0x55,0x00,                   //    Unit Exponent 0
			0xC0     ,    // End Collection
			0x09,0x73,    //    Usage Set Constant Force Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_CONSTREP+FFB_ID_OFFSET,         //    Report ID 5
			   0x09,0x22,         //    Usage Effect Block Index
			   0x15,0x01,         //    Logical Minimum 1
			   0x25,MAX_EFFECTS,         //    Logical Maximum 28h (40d)
			   0x35,0x01,         //    Physical Minimum 1
			   0x45,MAX_EFFECTS,         //    Physical Maximum 28h (40d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x70,         //    Usage Magnitude
			   0x16,0x00, 0x80,    //    Logical Minimum 7FFFh (-32767d)
			   0x26,0xff, 0x7f,    //    Logical Maximum 7FFFh (32767d)
			   0x36,0x00, 0x80,    //    Physical Minimum 7FFFh (-32767d)
			   0x46,0xff, 0x7f,    //    Physical Maximum 7FFFh (32767d)
			 0x75, 0x10,         //    Report Size 10h (16d)
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			0xC0     ,    //    End Collection
			0x09,0x74,    //    Usage Set Ramp Force Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_RAMPREP+FFB_ID_OFFSET,         //    Report ID 6
			   0x09,0x22,         //    Usage Effect Block Index
			   0x15,0x01,         //    Logical Minimum 1
			   0x25,MAX_EFFECTS,         //    Logical Maximum 28h (40d)
			   0x35,0x01,         //    Physical Minimum 1
			   0x45,MAX_EFFECTS,         //    Physical Maximum 28h (40d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x75,         //    Usage Ramp Start
			   0x09,0x76,         //    Usage Ramp End
			 0x16,0xF0,0xD8,    //    Logical Minimum D8F0h (-10000d)
			 0x26,0x10,0x27,    //    Logical Maximum 2710h (10000d)
			   0x36,0xF0,0xD8,    //    Physical Minimum D8F0h (-10000d)
			   0x46,0x10,0x27,    //    Physical Maximum 2710h (10000d)
			   0x75,0x10,         //    Report Size 16
			   0x95,0x02,         //    Report Count 2
			   0x91,0x02,         //    Output (Variable)
			0xC0     ,    //    End Collection
			0x09,0x68,    //    Usage Custom Force Data Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_CSTMREP+FFB_ID_OFFSET,         //    Report ID 7
			   0x09,0x22,         //    Usage Effect Block Index
			   0x15,0x01,         //    Logical Minimum 1
			   0x25,MAX_EFFECTS,         //    Logical Maximum 28h (40d)
			   0x35,0x01,         //    Physical Minimum 1
			   0x45,MAX_EFFECTS,         //    Physical Maximum 28h (40d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x6C,         //    Usage Custom Force Data Offset
			   0x15,0x00,         //    Logical Minimum 0
			   0x26,0x10,0x27,    //    Logical Maximum 2710h (10000d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0x10,0x27,    //    Physical Maximum 2710h (10000d)
			   0x75,0x10,         //    Report Size 10h (16d)
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x69,         //    Usage Custom Force Data
			   0x15,0x81,         //    Logical Minimum 81h (-127d)
			   0x25,0x7F,         //    Logical Maximum 7Fh (127d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x0C,         //    Report Count Ch (12d)
			   0x92,0x02,0x01,    //       Output (Variable, Buffered)
			0xC0     ,    //    End Collection
			0x09,0x66,    //    Usage Download Force Sample
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_SMPLREP+FFB_ID_OFFSET,         //    Report ID 8
			   0x05,0x01,         //    Usage Page Generic Desktop
			   0x09,0x30,         //    Usage X
			   0x09,0x31,         //    Usage Y
			   0x15,0x81,         //    Logical Minimum 81h (-127d)
			   0x25,0x7F,         //    Logical Maximum 7Fh (127d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x02,         //    Report Count 2
			   0x91,0x02,         //    Output (Variable)
			0xC0     ,   //    End Collection
			0x05,0x0F,   //    Usage Page Physical Interface
			0x09,0x77,   //    Usage Effect Operation Report
			0xA1,0x02,   //    Collection Datalink
			   0x85,HID_ID_EFOPREP+FFB_ID_OFFSET,    //    Report ID Ah (10d)
			   0x09,0x22,    //    Usage Effect Block Index
			   0x15,0x01,    //    Logical Minimum 1
			   0x25,MAX_EFFECTS,    //    Logical Maximum 28h (40d)
			   0x35,0x01,    //    Physical Minimum 1
			   0x45,MAX_EFFECTS,    //    Physical Maximum 28h (40d)
			   0x75,0x08,    //    Report Size 8
			   0x95,0x01,    //    Report Count 1
			   0x91,0x02,    //    Output (Variable)
			   0x09,0x78,    //    Usage Effect Operation
			   0xA1,0x02,    //    Collection Datalink
			      0x09,0x79,    //    Usage Op Effect Start
			      0x09,0x7A,    //    Usage Op Effect Start Solo
			      0x09,0x7B,    //    Usage Op Effect Stop
			      0x15,0x01,    //    Logical Minimum 1
			      0x25,0x03,    //    Logical Maximum 3
			      0x75,0x08,    //    Report Size 8
			      0x95,0x01,    //    Report Count 1
			      0x91,0x00,    //    Output
			   0xC0     ,         //    End Collection
			   0x09,0x7C,         //    Usage Loop Count
			   0x15,0x00,         //    Logical Minimum 0
			   0x26,0xFF,0x00,    //    Logical Maximum FFh (255d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)
			   0x91,0x02,         //    Output (Variable)
			0xC0     ,    //    End Collection
			0x09,0x90,    //    Usage PID Block Free Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_BLKFRREP+FFB_ID_OFFSET,    //    Report ID Bh (11d)
			   0x09,0x22,    //    Usage Effect Block Index
			   0x25,MAX_EFFECTS,    //    Logical Maximum 28h (40d)
			   0x15,0x01,    //    Logical Minimum 1
			   0x35,0x01,    //    Physical Minimum 1
			   0x45,MAX_EFFECTS,    //    Physical Maximum 28h (40d)
			   0x75,0x08,    //    Report Size 8
			   0x95,0x01,    //    Report Count 1
			   0x91,0x02,    //    Output (Variable)
			0xC0     ,    //    End Collection

			0x09,0x95,    //    Usage PID Device Control (0x96?)
			0xA1,0x02,    //    Collection Datalink
			0x85,HID_ID_CTRLREP+FFB_ID_OFFSET,    //    Report ID Ch (12d)
			0x09,0x96,    //    Usage PID Device Control (0x96?)
			0xA1,0x02,    //    Collection Datalink

			   0x09,0x97,    //    Usage DC Enable Actuators
			   0x09,0x98,    //    Usage DC Disable Actuators
			   0x09,0x99,    //    Usage DC Stop All Effects
			   0x09,0x9A,    //    Usage DC Device Reset
			   0x09,0x9B,    //    Usage DC Device Pause
			   0x09,0x9C,    //    Usage DC Device Continue



			   0x15,0x01,    //    Logical Minimum 1
			   0x25,0x06,    //    Logical Maximum 6
			   0x75,0x01,    //    Report Size 1
			   0x95,0x08,    //    Report Count 8
			   0x91,0x02,    //    Output

			0xC0     ,    //    End Collection
			0xC0     ,    //    End Collection
			0x09,0x7D,    //    Usage Device Gain Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_GAINREP+FFB_ID_OFFSET,         //    Report ID Dh (13d)
			   0x09,0x7E,         //    Usage Device Gain
			   0x15,0x00,         //    Logical Minimum 0
			   0x26,0xFF,0x00,    //    Logical Maximum FFh (255d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0x10,0x27,    //    Physical Maximum 2710h (10000d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			0xC0     ,            //    End Collection
			0x09,0x6B,    //    Usage Set Custom Force Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_SETCREP+FFB_ID_OFFSET,         //    Report ID Eh (14d)
			   0x09,0x22,         //    Usage Effect Block Index
			   0x15,0x01,         //    Logical Minimum 1
			   0x25,MAX_EFFECTS,         //    Logical Maximum 28h (40d)
			   0x35,0x01,         //    Physical Minimum 1
			   0x45,MAX_EFFECTS,         //    Physical Maximum 28h (40d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x6D,         //    Usage Sample Count
			   0x15,0x00,         //    Logical Minimum 0
			   0x26,0xFF,0x00,    //    Logical Maximum FFh (255d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)
			   0x75,0x08,         //    Report Size 8
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x09,0x51,         //    Usage Sample Period
			   0x66,0x03,0x10,    //    Unit 1003h (4099d)
			   0x55,0xFD,         //    Unit Exponent FDh (253d)
			   0x15,0x00,         //    Logical Minimum 0
			   0x26,0xFF,0x7F,    //    Logical Maximum 7FFFh (32767d)
			   0x35,0x00,         //    Physical Minimum 0
			   0x46,0xFF,0x7F,    //    Physical Maximum 7FFFh (32767d)
			   0x75,0x10,         //    Report Size 10h (16d)
			   0x95,0x01,         //    Report Count 1
			   0x91,0x02,         //    Output (Variable)
			   0x55,0x00,         //    Unit Exponent 0
			   0x66,0x00,0x00,    //    Unit 0
			0xC0     ,    //    End Collection
			0x09,0xAB,    //    Usage Create New Effect Report
			0xA1,0x02,    //    Collection Datalink
			   0x85,HID_ID_NEWEFREP+FFB_ID_OFFSET,    //    Report ID 1
			   0x09,0x25,    //    Usage Effect Type
			   0xA1,0x02,    //    Collection Datalink
			 0x09, HID_USAGE_CONST,    //    Usage ET Constant Force
			 0x09, HID_USAGE_RAMP,    //    Usage ET Ramp
			 0x09, HID_USAGE_SQUR,    //    Usage ET Square
			 0x09, HID_USAGE_SINE,    //    Usage ET Sine
			 0x09, HID_USAGE_TRNG,    //    Usage ET Triangle
			 0x09, HID_USAGE_STUP,    //    Usage ET Sawtooth Up
			 0x09, HID_USAGE_STDN,    //    Usage ET Sawtooth Down
			 0x09, HID_USAGE_SPRNG,    //    Usage ET Spring
			 0x09, HID_USAGE_DMPR,    //    Usage ET Damper
			 0x09, HID_USAGE_INRT,    //    Usage ET Inertia
			 0x09, HID_USAGE_FRIC,    //    Usage ET Friction
			 0x09, 0x28,    //    Usage ET Custom Force Data
			   0x25,0x0C,    //    Logical Maximum Ch (12d)
			   0x15,0x01,    //    Logical Minimum 1
			   0x35,0x01,    //    Physical Minimum 1
			   0x45,0x0C,    //    Physical Maximum Ch (12d)
			   0x75,0x08,    //    Report Size 8
			   0x95,0x01,    //    Report Count 1
			   0xB1,0x00,    //    Feature
			0xC0     ,    // End Collection
			0x05,0x01,         //    Usage Page Generic Desktop
			0x09,0x3B,         //    Usage Reserved
			0x15,0x00,         //    Logical Minimum 0
			0x26,0xFF,0x01,    //    Logical Maximum 1FFh (511d)
			0x35,0x00,         //    Physical Minimum 0
			0x46,0xFF,0x01,    //    Physical Maximum 1FFh (511d)
			0x75,0x0A,         //    Report Size Ah (10d)
			0x95,0x01,         //    Report Count 1
			0xB1,0x02,         //    Feature (Variable)
			0x75,0x06,         //    Report Size 6
			0xB1,0x01,         //    Feature (Constant)
			0xC0     ,    //    End Collection
			0x05,0x0F,    //    Usage Page Physical Interface
			0x09,0x89,    //    Usage Block Load Report
			0xA1,0x02,    //    Collection Datalink
			0x85,HID_ID_BLKLDREP+FFB_ID_OFFSET,    //    Report ID 0x12
			0x09,0x22,    //    Usage Effect Block Index
			0x25,MAX_EFFECTS,    //    Logical Maximum 28h (40d)
			0x15,0x01,    //    Logical Minimum 1
			0x35,0x01,    //    Physical Minimum 1
			0x45,MAX_EFFECTS,    //    Physical Maximum 28h (40d)
			0x75,0x08,    //    Report Size 8
			0x95,0x01,    //    Report Count 1
			0xB1,0x02,    //    Feature (Variable)
			0x09,0x8B,    //    Usage Block Load Status
			0xA1,0x02,    //    Collection Datalink
			   0x09,0x8C,    //    Usage Block Load Success
			   0x09,0x8D,    //    Usage Block Load Full
			   0x09,0x8E,    //    Usage Block Load Error
			   0x25,0x03,    //    Logical Maximum 3
			   0x15,0x01,    //    Logical Minimum 1
			   0x35,0x01,    //    Physical Minimum 1
			   0x45,0x03,    //    Physical Maximum 3
			   0x75,0x08,    //    Report Size 8
			   0x95,0x01,    //    Report Count 1
			   0xB1,0x00,    //    Feature
			0xC0     ,                   // End Collection
			0x09,0xAC,                   //    Usage Undefined
			0x15,0x00,                   //    Logical Minimum 0
			0x27,0xFF,0xFF,0x00,0x00,    //    Logical Maximum FFFFh (65535d)
			0x35,0x00,                   //    Physical Minimum 0
			0x47,0xFF,0xFF,0x00,0x00,    //    Physical Maximum FFFFh (65535d)
			0x75,0x10,                   //    Report Size 10h (16d)
			0x95,0x01,                   //    Report Count 1
			0xB1,0x00,                   //    Feature
			0xC0     ,    //    End Collection
			0x09,0x7F,    //    Usage PID Pool Report
			0xA1,0x02,    //    Collection Datalink
			0x85,HID_ID_POOLREP+FFB_ID_OFFSET,                   //    Report ID 0x13
			0x09,0x80,                   //    Usage RAM Pool size
			0x75,0x10,                   //    Report Size 10h (16d)
			0x95,0x01,                   //    Report Count 1
			0x15,0x00,                   //    Logical Minimum 0
			0x35,0x00,                   //    Physical Minimum 0
			0x27,0xFF,0xFF,0x00,0x00,    //    Logical Maximum FFFFh (65535d)
			0x47,0xFF,0xFF,0x00,0x00,    //    Physical Maximum FFFFh (65535d)
			0xB1,0x02,                   //    Feature (Variable)
			0x09,0x83,                   //    Usage Simultaneous Effects Max
			0x26,0xFF,0x00,              //    Logical Maximum FFh (255d)
			0x46,0xFF,0x00,              //    Physical Maximum FFh (255d)
			0x75,0x08,                   //    Report Size 8
			0x95,0x01,                   //    Report Count 1
			0xB1,0x02,                   //    Feature (Variable)
			0x09,0xA9,                   //    Usage Device Managed Pool
			0x09,0xAA,                   //    Usage Shared Parameter Blocks
			0x75,0x01,                   //    Report Size 1
			0x95,0x02,                   //    Report Count 2
			0x15,0x00,                   //    Logical Minimum 0
			0x25,0x01,                   //    Logical Maximum 1
			0x35,0x00,                   //    Physical Minimum 0
			0x45,0x01,                   //    Physical Maximum 1
			0xB1,0x02,                   //    Feature (Variable)
			0x75,0x06,                   //    Report Size 6
			0x95,0x01,                   //    Report Count 1
			0xB1,0x03,                   //    Feature (Constant, Variable)
			0xC0, //    End Collection

  /* USER CODE END 0 */
  0xC0    /*     END_COLLECTION	             */
};

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Exported_Variables USBD_CUSTOM_HID_Exported_Variables
  * @brief Public variables.
  * @{
  */

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */
/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Private_FunctionPrototypes USBD_CUSTOM_HID_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t CUSTOM_HID_Init_FS(void);
static int8_t CUSTOM_HID_DeInit_FS(void);
static int8_t CUSTOM_HID_OutEvent_FS(uint8_t* report);
static int8_t CUSTOM_HID_GetEvent_FS(USBD_SetupReqTypedef* req,uint8_t** return_buf);

/**
  * @}
  */

USBD_CUSTOM_HID_ItfTypeDef USBD_CustomHID_fops_FS =
{
  CUSTOM_HID_ReportDesc_FS,
  CUSTOM_HID_Init_FS,
  CUSTOM_HID_DeInit_FS,
  CUSTOM_HID_OutEvent_FS, // must be modified to pass the full report
  CUSTOM_HID_GetEvent_FS
};

/** @defgroup USBD_CUSTOM_HID_Private_Functions USBD_CUSTOM_HID_Private_Functions
  * @brief Private functions.
  * @{
  */

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes the CUSTOM HID media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_Init_FS(void)
{
  /* USER CODE BEGIN 4 */
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  DeInitializes the CUSTOM HID media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_DeInit_FS(void)
{
  /* USER CODE BEGIN 5 */
  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Manage the CUSTOM HID class events
  * @param  report: the report
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_OutEvent_FS(uint8_t* report)
{
  /* USER CODE BEGIN 6 */
  USBD_OutEvent_HID(report);
  return (USBD_OK);
  /* USER CODE END 6 */
}

/* USER CODE BEGIN 7 */

static int8_t CUSTOM_HID_GetEvent_FS(USBD_SetupReqTypedef* req,uint8_t** return_buf)
{
  /* USER CODE BEGIN 6 */
  USBD_GetEvent_HID((req->wValue & 0xff),req->wLength,return_buf);
  return (USBD_OK);
  /* USER CODE END 6 */
}
/**
  * @brief  Send the report to the Host
  * @param  report: The report to be sent
  * @param  len: The report length
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
/*
static int8_t USBD_CUSTOM_HID_SendReport_FS(uint8_t *report, uint16_t len)
{
  return USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report, len);
}
*/
/* USER CODE END 7 */

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

