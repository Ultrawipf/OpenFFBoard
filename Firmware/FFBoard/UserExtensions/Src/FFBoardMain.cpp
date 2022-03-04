/*
 * FFBoardMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include "FFBoardMain.h"

#include <thread.hpp>
#include "constants.h"
#include "usb_descriptors.h"
#include "cdc_device.h"

ClassIdentifier FFBoardMain::info ={.name = "Basic (Failsafe)" ,.id=0};

char FFBoardMain::cdcbuf[64];



FFBoardMain::FFBoardMain() : CommandHandler(CMDCLSTR_MAIN,CMDCLSID_MAIN,0), commandThread(std::make_unique<FFBoardMainCommandThread>(this)){
	CommandHandler::registerCommands(); // Register the internal system commands. Mainclasses always have these commands and should not call this function anywhere else
}

const ClassIdentifier FFBoardMain::getInfo(){
	return info;
}

/**
 * Called when data is received on the CDC port
 */
void FFBoardMain::cdcRcv(char* Buf, uint32_t *Len){

	cdcCmdInterface->addBuf(Buf, Len);
}

/**
 * Called by the CDC serial port when data is ready
 */
void FFBoardMain::cdcRcvReady(uint8_t itf){

	uint32_t bufferFree = std::min<uint32_t>(cdcCmdInterface->bufferCapacity(),sizeof(this->cdcbuf));
	uint32_t count = tud_cdc_n_read(itf,this->cdcbuf, bufferFree);
	this->cdcRcv(this->cdcbuf,&count);
}



CommandStatus FFBoardMain::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	return CommandStatus::NOT_FOUND;
}





/**
 * Called during the startup
 * Should initialize a USBdevice and call registerUsb()
 */
void FFBoardMain::usbInit(){
	// Create the usb config if this is not overridden
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_conf,&usb_ffboard_strings_default);
	usbdev->registerUsb();
}

/**
 * Called in the default thread with no delay
 * Can be reimplemented by custom main classes but should not block
 */
void FFBoardMain::update(){

}

/**
 * Called when the usb port is disconnected
 */
void FFBoardMain::usbSuspend(){

}

/**
 * Called when the usb port is connected
 */
void FFBoardMain::usbResume(){

}


std::string FFBoardMain::getHelpstring(){
	return "Failsafe mainclass with no features. Choose a different mainclass. sys.lsmain to get a list";
}

FFBoardMain::~FFBoardMain() {

}

