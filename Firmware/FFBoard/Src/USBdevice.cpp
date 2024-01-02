/*
 * USBdevice.cpp
 *
 *  Created on: 19.02.2021
 *      Author: Yannick
 */

#include "USBdevice.h"
#include "tusb.h"

uint16_t _desc_str[USB_STRING_DESC_BUF_SIZE]; // String buffer

USBdevice::USBdevice(const tusb_desc_device_t* deviceDesc,const uint8_t (*confDesc),const usb_string_desc_t* strings, uint8_t appendSerial) :
Thread("USB", 256, 40), desc_device(deviceDesc), desc_conf(confDesc), string_desc(strings),appendSerial(appendSerial) {

}

USBdevice::~USBdevice() {

}

/**
 * Registers the usb callbacks and starts the tinyusb main thread
 */
void USBdevice::registerUsb(){
	// Global callback pointer
	extern USBdevice* usb_device;
	usb_device = this;

	this->Start();
}

void USBdevice::Run(){
	tusb_init();
	while(1){
		tud_task(); // Run until no usb events left
	}
}

/**
 *  Generates a unique id string from the hardware id
 */
std::string USBdevice::getUsbSerial(){
	std::string serial = std::to_string(HAL_GetUIDw0()) + std::to_string(HAL_GetUIDw1()) + std::to_string(HAL_GetUIDw2());

	return serial;
}

const uint8_t* USBdevice::getUsbDeviceDesc(){
	return (uint8_t*)desc_device;
}

const uint8_t* USBdevice::getUsbConfigurationDesc(uint8_t index){
	return desc_conf;
}

/**
 * Returns a usb formatted string from the stringtable
 */
uint16_t* USBdevice::getUsbStringDesc(uint8_t index,uint16_t langid){
	(void) langid;
	uint16_t chr_count = 0;
	if (index == 0) // Language
	{
		_desc_str[1] = string_desc->langId;
		chr_count = 1;
	}else{
		std::string field;
		if(index == desc_device->iSerialNumber){
			field = getUsbSerial();
		}else if(index == desc_device->iProduct){
			field = string_desc->product;
		}else if(index == desc_device->iManufacturer){
			field = string_desc->manufacturer;
		}else if(index > 3 && ((index - 4) < (int)string_desc->interfaces.size())){
			field = string_desc->interfaces[index-4];
			if(appendSerial){
				field += "(";
				field.append(getUsbSerial().substr(0, appendSerial));
				field += ")";
			}
		}else{
			return NULL;
		}
		chr_count = std::min<uint8_t>(USB_STRING_DESC_BUF_SIZE-1,field.length());
		// Convert ASCII string into UTF-16
		for(uint8_t i=0; i < chr_count; i++)
		{
		  _desc_str[1+i] = field[i];
		}
	}

	// first byte is length (including header), second byte is string type
	_desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

	return _desc_str;
}



