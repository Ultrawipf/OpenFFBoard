/*
 * FFBShifter.cpp
 *
 *  Created on: 02.03.2023
 *      Author: Yannick
 */

#include "FFBShifter.h"
#ifdef FFBSHIFTER

// Unique identifier for listing
ClassIdentifier FFBShifter::info = {
		 .name = "FFB Shifter" ,
		 .id=CLSID_MAIN_FFBSHIFTER,
 };

const ClassIdentifier FFBShifter::getInfo(){
	return info;
}


FFBShifter::FFBShifter() : FFBHIDMain(2){
	FFBHIDMain::setFFBEffectsCalc(ffb, static_cast<std::shared_ptr<EffectsCalculatorItf>>(effects_calc));

}

FFBShifter::~FFBShifter() {
	// TODO Auto-generated destructor stub
}

void FFBShifter::usbInit(){
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_hid_conf_gamepad,&usb_ffboard_strings_default);
	FFBHIDMain::UsbHidHandler::setHidDesc(hid_gamepad_desc);

	usbdev->registerUsb();
}

// Shifter effects. Create the snappy positions here

FFBShifterEffects::FFBShifterEffects(){

}

FFBShifterEffects::~FFBShifterEffects(){

}

void FFBShifterEffects::setActive(bool active){
	this->active = active;
}


void FFBShifterEffects::calculateEffects(std::vector<std::unique_ptr<Axis>> &axes){
	// Will have 2 axes created via the FFBHIDMain
}

#endif
