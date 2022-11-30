/*
 * FFBHIDExt.cpp
 *
 *  Created on: 30.11.2022
 *      Author: Yannick
 */


#include "FFBHIDExt.h"

#ifdef FFBHIDEXT

#include "usb_hid_ffb_desc.h"

// Unique identifier for listing
ClassIdentifier FFBHIDExt::info = {
		 .name = "HID Gamepad (Ext FFB)" ,
		 .id=CLSID_MAIN_FFBEXT,
 };

const ClassIdentifier FFBHIDExt::getInfo(){
	return info;
}


FFBHIDExt::FFBHIDExt() :
		FFBHIDMain(1,ffb,effects_calc)
{

}

FFBHIDExt::~FFBHIDExt() {

}



void FFBHIDExt::usbInit(){
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_hid_conf_gamepad,&usb_ffboard_strings_default);
	FFBHIDMain::UsbHidHandler::setHidDesc(hid_gamepad_desc);
	effects_calc->setDirectionEnableMask(0x04); // TODO change direction system?

	usbdev->registerUsb();
}

#endif
