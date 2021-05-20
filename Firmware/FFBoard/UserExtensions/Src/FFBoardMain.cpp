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

ClassIdentifier FFBoardMain::info ={.name = "Basic" , .id=0, .unique = '0'};



FFBoardMain::FFBoardMain() : systemCommands(std::make_unique<FFBoardMainCommandThread>(this)){

}

const ClassIdentifier FFBoardMain::getInfo(){
	return info;
}


void FFBoardMain::cdcRcv(char* Buf, uint32_t *Len){
//	if(systemCommands->addBuf(Buf, Len,!usb_busy_retry)){
//		 // resume command execution
//			//systemCommands->Resume();
//	}
	systemCommands->addBuf(Buf, Len,!usb_busy_retry);
}

ParseStatus FFBoardMain::command(ParsedCommand *cmd,std::string* reply){

	return ParseStatus::NOT_FOUND;
}





/*
 * Global callback if cdc transfer is finished. Used to retry a failed transfer
 */
void FFBoardMain::cdcFinished(uint8_t itf = 0){
	if(usb_busy_retry && this->cdcRemaining.length() > 0){
		cdcSend(&this->cdcRemaining,&this->cdcRemaining, itf); // Retry with remaining string
	}
}

void FFBoardMain::usbInit(){
	// Create the usb config if this is not overridden
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_conf,&usb_ffboard_strings_default);
	usbdev->registerUsb();
}

// Virtual stubs
void FFBoardMain::update(){

}

void FFBoardMain::usbSuspend(){

}
void FFBoardMain::usbResume(){

}

void FFBoardMain::parserDone(std::string* reply, FFBoardMainCommandThread* parser){
	if(parser == this->systemCommands.get()){
		cdcSend(reply,&this->cdcRemaining, 0);
	}
}

uint16_t FFBoardMain::cdcSend(std::string* reply, std::string* remaining,uint8_t itf){

	uint16_t cdc_sent = tud_cdc_n_write(itf,reply->c_str(), std::min<uint16_t>(reply->length(),CFG_TUD_CDC_TX_BUFSIZE));
	tud_cdc_n_write_flush(itf);
	// If we can't write the whole reply copy remainder to send later
	if(cdc_sent < reply->length()){
		cdcRemaining.assign(reply->substr(cdc_sent));
		usb_busy_retry = true;
	}else{
		usb_busy_retry = false;
		this->cdcRemaining.clear();
	}
	return cdc_sent;
}

std::string FFBoardMain::getHelpstring(){
	return FFBoardMainCommandThread::getHelpstring();
}

FFBoardMain::~FFBoardMain() {

}

