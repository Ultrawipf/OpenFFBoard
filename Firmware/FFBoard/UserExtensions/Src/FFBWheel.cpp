/*
 * FFBWheel.cpp
 *
 *  Created on: 29.03.2022
 *      Author: Yannick
 */


#include "FFBWheel.h"

#ifdef FFBWHEEL

#include "usb_hid_ffb_desc.h"

// Unique identifier for listing
ClassIdentifier FFBWheel::info = {
		 .name = "FFB Wheel (1 Axis)" ,
		 .id=CLSID_MAIN_FFBWHEEL,
 };

const ClassIdentifier FFBWheel::getInfo(){
	return info;
}


FFBWheel::FFBWheel() :
		FFBHIDMain(1,FFBWHEEL_32B_MODE)
{
	FFBHIDMain::setFFBEffectsCalc(ffb, effects_calc);
}

FFBWheel::~FFBWheel() {

}


void FFBWheel::usbInit(){
#ifdef HIDAXISRES_USE_32B_DESC
#ifdef FFBWHEEL_USE_1AXIS_DESC
const uint8_t* usbconf = usb_cdc_hid_conf_1axis_32b;
const uint8_t* ffbdesc = hid_1ffb_desc_32b;
#else
const uint8_t* ffbdesc = hid_2ffb_desc_32b;
const uint8_t* usbconf = usb_cdc_hid_conf_2axis_32b;
#endif
#else // ELSE 32B
#ifdef FFBWHEEL_USE_1AXIS_DESC
const uint8_t* usbconf = usb_cdc_hid_conf_1axis;
const uint8_t* ffbdesc = hid_1ffb_desc;
#else
const uint8_t* ffbdesc = hid_2ffb_desc;
const uint8_t* usbconf = usb_cdc_hid_conf_2axis;
#endif
#endif

	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usbconf,&usb_ffboard_strings_default);

	FFBHIDMain::UsbHidHandler::setHidDesc(ffbdesc);
	static_cast<HidFFB*>(ffb.get())->setDirectionEnableMask(0x04);

	usbdev->registerUsb();
}

#endif
