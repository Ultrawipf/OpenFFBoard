/*
 * UsbHidHandler.cpp
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#include "UsbHidHandler.h"
#include "global_callbacks.h"
#include "hid_device.h"

uint8_t* UsbHidHandler::hid_desc = nullptr;
UsbHidHandler* UsbHidHandler::globalHidHandler = nullptr;

UsbHidHandler::UsbHidHandler() {

}

UsbHidHandler::~UsbHidHandler() {

}


// Returns length
uint16_t UsbHidHandler::hidGet(uint8_t report_id, hid_report_type_t report_type,uint8_t* buffer, uint16_t reqlen){
	return 0;
}

void UsbHidHandler::hidOut(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize){

}

void UsbHidHandler::registerHidCallback(){
	globalHidHandler = this;
}

// Class specific report callbacks
void UsbHidHandler::setHidDesc(const uint8_t* desc){
	UsbHidHandler::hid_desc = (uint8_t*)desc;
}
const uint8_t* UsbHidHandler::getHidDesc(){
	return UsbHidHandler::hid_desc;
}
