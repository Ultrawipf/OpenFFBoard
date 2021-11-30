/*
 * HidCommandHandler.cpp
 *
 *  Created on: 15.03.2021
 *      Author: Yannick
 */

#include "HidCommandHandler.h"
#include "global_callbacks.h"


// TODO REMOVE and replace with new command system
std::vector<HidCommandHandler*> HidCommandHandler::hidCmdHandlers; // called only for custom cmd report ids

HidCommandHandler::HidCommandHandler(){
	addCallbackHandler(HidCommandHandler::hidCmdHandlers,this);
}

HidCommandHandler::~HidCommandHandler() {
	removeCallbackHandler(HidCommandHandler::hidCmdHandlers,this);
}

/*
 * Send a custom transfer with the vendor defined IN report
 */
bool HidCommandHandler::sendHidCmd(HID_Custom_Data_t* data){
	return tud_hid_report(HID_ID_CUSTOMCMD, (void const*) data, sizeof(HID_Custom_Data_t));
}

void HidCommandHandler::processHidCommand(HID_Custom_Data_t *data){

}
