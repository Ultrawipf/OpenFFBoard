/*
 * FFBWheel_usb_init.h
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#ifndef INC_FFBWHEEL_USB_INIT_H_
#define INC_FFBWHEEL_USB_INIT_H_

extern "C"{
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

#include "constants.h"
#include "usbd_composite.h"
#include "usbd_customhid.h"
}


void usbInit_HID_Wheel();

#endif /* INC_FFBWHEEL_USB_INIT_H_ */
