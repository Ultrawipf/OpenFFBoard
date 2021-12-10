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

ClassIdentifier FFBoardMain::info ={.name = "Basic" ,.id=0};



FFBoardMain::FFBoardMain() : CommandHandler(CMDCLSTR_MAIN,CMDCLSID_MAIN,0), commandThread(std::make_unique<FFBoardMainCommandThread>(this)){
	CommandHandler::registerCommands(); // Register the internal system commands. Mainclasses always have these commands and should not call this function anywhere else
}

const ClassIdentifier FFBoardMain::getInfo(){
	return info;
}

/**
 * Called by the CDC serial port when data is received
 */
void FFBoardMain::cdcRcv(char* Buf, uint32_t *Len){

	cdcCmdInterface->addBuf(Buf, Len);
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


//TODO move to cdc class
//void FFBoardMain::parserDone(std::string* reply, FFBoardMainCommandThread* parser){
//	if(parser == this->systemCommands.get()){
//		cdcSend(reply,&this->cdcRemaining, 0);
//	}
//}
// TODO move to class
//uint16_t FFBoardMain::cdcSend(std::string* reply, std::string* remaining,uint8_t itf){
//
//	uint16_t cdc_sent = tud_cdc_n_write(itf,reply->c_str(), std::min<uint16_t>(reply->length(),CFG_TUD_CDC_TX_BUFSIZE));
//	tud_cdc_n_write_flush(itf);
//	// If we can't write the whole reply copy remainder to send later
//	if(cdc_sent < reply->length()){
//		cdcRemaining.assign(reply->substr(cdc_sent));
//		usb_busy_retry = true;
//	}else{
//		usb_busy_retry = false;
//		this->cdcRemaining.clear();
//	}
//	return cdc_sent;
//}

std::string FFBoardMain::getHelpstring(){
	return "Failsafe mainclass with no features. Choose a different mainclass. sys.lsmain to get a list";
}

FFBoardMain::~FFBoardMain() {

}

