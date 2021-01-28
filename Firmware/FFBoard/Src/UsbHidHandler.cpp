/*
 * UsbHidHandler.cpp
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#include "UsbHidHandler.h"
#include "global_callbacks.h"

UsbHidHandler::UsbHidHandler() {
	/*
	 * By default it will receive only the custom feature reports
	 * You can override that to receive every HID command
	 */
	extern std::vector<UsbHidHandler*> hidCmdHandlers;
	addCallbackHandler(&hidCmdHandlers,this);
}

UsbHidHandler::~UsbHidHandler() {
	extern std::vector<UsbHidHandler*> hidCmdHandlers;
	removeCallbackHandler(&hidCmdHandlers,this);
}


void UsbHidHandler::hidOutCmd(HID_Custom_Data_t* data){

}

void UsbHidHandler::hidGet(uint8_t id,uint16_t len,uint8_t** return_buf){

}

void UsbHidHandler::hidOut(uint8_t* report){

}

void UsbHidHandler::registerHidCallback(){
	extern UsbHidHandler* globalHidHandler;
	globalHidHandler = this;
}
