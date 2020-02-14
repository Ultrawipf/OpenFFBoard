/*
 * UsbHidHandler.cpp
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#include "UsbHidHandler.h"
#include "global_callbacks.h"

UsbHidHandler::UsbHidHandler() {
	// Don't auto register. Call registerCallback()
	// There should only be one hid handler for now
}

UsbHidHandler::~UsbHidHandler() {
	// TODO Auto-generated destructor stub
}



void UsbHidHandler::hidGet(uint8_t id,uint16_t len,uint8_t** return_buf){

}

void UsbHidHandler::hidOut(uint8_t* report){

}

void UsbHidHandler::registerHidCallback(){
	extern UsbHidHandler* globalHidHandler;
	globalHidHandler = this;
}
