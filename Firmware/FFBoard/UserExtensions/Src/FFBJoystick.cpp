/*
 * FFBWheel.cpp
 *
 *  Created on: 29.03.2022
 *      Author: Yannick
 */

#include "FFBJoystick.h"
#ifdef FFBJOYSTICK


#include "usb_hid_ffb_desc.h"

// Unique identifier for listing
ClassIdentifier FFBJoystick::info = {
		 .name = "FFB Joystick (2 Axis)" ,
		 .id=CLSID_MAIN_FFBJOY,
 };

const ClassIdentifier FFBJoystick::getInfo(){
	return info;
}


FFBJoystick::FFBJoystick() :
		FFBHIDMain(2)
{
	FFBHIDMain::setFFBEffectsCalc(ffb, effects_calc);
}

FFBJoystick::~FFBJoystick() {

}



void FFBJoystick::usbInit(){
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_hid_conf_2axis,&usb_ffboard_strings_default);
	FFBHIDMain::UsbHidHandler::setHidDesc(hid_2ffb_desc);
	usbdev->registerUsb();
}
#endif
