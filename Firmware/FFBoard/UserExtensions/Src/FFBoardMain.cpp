/*
 * FFBoardMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include "FFBoardMain.h"

#include <thread.hpp>
#include "constants.h"
#include "usbd_desc.h"

#include "cdc_device.h"

ClassIdentifier FFBoardMain::info ={.name = "Basic" , .id=0};



FFBoardMain::FFBoardMain() : systemCommands(new FFBoardMainCommandThread(this)){
	//this->Start(); // Thread starts when parser is ready
}

const ClassIdentifier FFBoardMain::getInfo(){
	return info;
}


void FFBoardMain::cdcRcv(char* Buf, uint32_t *Len){
	if(systemCommands->addBuf(Buf, Len,!usb_busy_retry)){
		 // resume command execution
		systemCommands->ResumeFromISR();
	}
}

ParseStatus FFBoardMain::command(ParsedCommand *cmd,std::string* reply){

	return ParseStatus::NOT_FOUND;
}



/*
 * Formats a serial reply in command form
 */
void FFBoardMain::sendSerial(std::string cmd,std::string string){
	std::string reply = ">" + cmd + ":" + string + "\n";
	tud_cdc_n_write(0,reply.c_str(), reply.length());
}

/*
 * Sends log info
 */
void FFBoardMain::logSerial(std::string* string){
	std::string reply = "!" + *string + "\n";
	tud_cdc_n_write(0,reply.c_str(), reply.length());
}



/*
 * Global callback if cdc transfer is finished. Used to retry a failed transfer
 */
void FFBoardMain::cdcFinished(){
//	if(this->usb_busy_retry){
//		if(CDC_Transmit_FS(systemCommands->cmd_reply.c_str(), systemCommands->cmd_reply.length()) == USBD_OK){
//			this->usb_busy_retry = false;
//		}
//	}
}

void FFBoardMain::usbInit(USBD_HandleTypeDef* hUsbDeviceFS){

	USBD_Init(hUsbDeviceFS, &FS_Desc, DEVICE_FS);
	USBD_RegisterClass(hUsbDeviceFS, &USBD_CDC);
	USBD_CDC_RegisterInterface(hUsbDeviceFS, &USBD_Interface_fops_FS);
	USBD_Start(hUsbDeviceFS);
}

// Virtual stubs
void FFBoardMain::update(){

}
void FFBoardMain::SOF(){

}
void FFBoardMain::usbSuspend(){

}
void FFBoardMain::usbResume(){

}

void FFBoardMain::parserDone(std::string* reply, FFBoardMainCommandThread* parser){
	if(parser == this->systemCommands){
		if(tud_cdc_n_write(0,reply->c_str(), reply->length()) == 0){
			this->usb_busy_retry = true;
		}
	}
}

std::string FFBoardMain::getHelpstring(){
	return FFBoardMainCommandThread::getHelpstring();
}

FFBoardMain::~FFBoardMain() {
	delete systemCommands;
}

