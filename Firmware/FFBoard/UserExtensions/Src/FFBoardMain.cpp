/*
 * FFBoardMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include "FFBoardMain.h"

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>

#include "usbd_core.h"
#include "usbd_cdc.h"
#include "constants.h"
#include "usbd_composite.h"
#include "usbd_desc.h"
#include "mainclass_chooser.h"
#include "eeprom_addresses.h"
#include "flash_helpers.h"
#include "eeprom.h"
#include "voltagesense.h"
#include "global_callbacks.h"
#include "PersistentStorage.h"

#include "ClassChooser.h"
extern ClassChooser<FFBoardMain> mainchooser;

ClassIdentifier FFBoardMain::info ={.name = "Basic" , .id=0};

FFBoardMain::FFBoardMain() {

}

const ClassIdentifier FFBoardMain::getInfo(){
	return info;
}


void FFBoardMain::cdcRcv(char* Buf, uint32_t *Len){
	if(this->parser.add(Buf, Len)){
		this->parserReady = true; // Set flag to call outside of interrupt later
	}
}

ParseStatus FFBoardMain::command(ParsedCommand *cmd,std::string* reply){

	return ParseStatus::NOT_FOUND;
}

void FFBoardMain::updateSys(){
	if(this->parserReady){
		this->parserReady = false;
		executeCommands(this->parser.parse()); // Don't call this in interrupts!
	}
}

/*
 * Prints a formatted flash dump to the reply string
 */
void FFBoardMain::printFlashDump(std::string *reply){
	std::vector<std::tuple<uint16_t,uint16_t>> result;

	Flash_Dump(&result);
	uint16_t adr;
	uint16_t val;

	for(auto entry : result){
		std::tie(adr,val) = entry;
		*reply += std::to_string(adr) + ":" + std::to_string(val) + "\n";
	}

}


ParseStatus FFBoardMain::executeSysCommand(ParsedCommand* cmd,std::string* reply){
	ParseStatus flag = ParseStatus::OK;

	if(cmd->cmd == "help"){
		*reply += parser.helpstring;
		extern std::vector<CommandHandler*> cmdHandlers;
		for(CommandHandler* handler : cmdHandlers){
			*reply += handler->getHelpstring();
		}

	}else if(cmd->cmd == "save"){
		extern std::vector<PersistentStorage*> flashHandlers;
		for(PersistentStorage* handler : flashHandlers){
			handler->saveFlash();
		}

	}else if(cmd->cmd == "reboot"){
		NVIC_SystemReset();
	}else if(cmd->cmd == "dfu"){ // Reboot into DFU bootloader mode
		RebootDFU();
	}else if(cmd->cmd == "vint"){
		if(cmd->type==CMDtype::get){
			*reply+=std::to_string(getIntV());
		}

	}else if(cmd->cmd == "vext"){
		if(cmd->type==CMDtype::get){
			*reply+=std::to_string(getExtV());
		}

	}else if(cmd->cmd == "swver"){
		*reply += (SW_VERSION);

	}else if(cmd->cmd == "hwtype"){
		*reply += (HW_TYPE);

	}else if(cmd->cmd == "flashdump"){
		printFlashDump(reply);

	}else if(cmd->cmd == "flashraw"){ // Set and get flash eeprom emulation values
		if(cmd->type == CMDtype::setat){
			Flash_Write(cmd->adr, cmd->val);

		}else if(cmd->type == CMDtype::getat){
			uint16_t val;
			if(Flash_Read(cmd->adr,&val)){
				*reply+=std::to_string(val);
			}
			else{
				flag = ParseStatus::ERR;
			}
		}

	}else if(cmd->type!=CMDtype::set &&cmd->cmd == "lsmain"){
		*reply += mainchooser.printAvailableClasses();

	}else if(cmd->cmd == "id"){ // Report id of main class
		*reply+=std::to_string(this->getInfo().id);

	}else if(cmd->cmd == "mallinfo"){ // Report id of main class
		struct mallinfo info = mallinfo();
		*reply+="Usage: ";
		*reply+=std::to_string(info.uordblks);
		*reply+=" Size: ";
		*reply+=std::to_string(info.arena);

	}else if(cmd->cmd == "lsactive"){ // Prints all active command handlers that have a name
		extern std::vector<CommandHandler*> cmdHandlers;
		for(CommandHandler* handler : cmdHandlers){
			if(handler->hasCommands()){
				ClassIdentifier i = handler->getInfo();
				if(!i.hidden)
					*reply += std::string(i.name) + ":" + std::to_string(i.id) + "\n";
			}
		}

	}else if(cmd->cmd == "main"){
		if(cmd->type == CMDtype::get || cmd->type == CMDtype::none){
			uint16_t buf=this->getInfo().id;
			Flash_Read(ADR_CURRENT_CONFIG, &buf);
			*reply+=std::to_string(buf);

		}else if(cmd->type == CMDtype::set){
			if(mainchooser.isValidClassId(cmd->val)){
				Flash_Write(ADR_CURRENT_CONFIG, (uint16_t)cmd->val);
				if(cmd->val != this->getInfo().id){
					NVIC_SystemReset(); // Reboot
				}
			}else if(cmd->type == CMDtype::err){
				flag = ParseStatus::ERR;
			}
		}
	}else if(cmd->cmd == "format"){
		if(cmd->type == CMDtype::set && cmd->val==1){
			HAL_FLASH_Unlock();
			EE_Format();
			HAL_FLASH_Lock();
		}else{
			*reply += "format=1 will ERASE ALL stored variables. Be careful!";
		}
	}else{
		// No command found
		flag = ParseStatus::NOT_FOUND;
	}

	return flag;
}

/*
 * Executes parsed commands and calls other command handlers.
 * Not global so it can be overridden by main classes to change behaviour or suppress outputs.
 */
void FFBoardMain::executeCommands(std::vector<ParsedCommand> commands){
	if(!usb_busy_retry){
		this->cmd_reply.clear();
	}
	extern std::vector<CommandHandler*> cmdHandlers;

	for(ParsedCommand cmd : commands){
		ParseStatus status = ParseStatus::NOT_FOUND;
		if(cmd.cmd.empty())
			continue; // Empty command


		this->cmd_reply+= ">" + cmd.rawcmd + ":"; // Start marker. Echo command

		std::string reply; // Reply of current command
		status = executeSysCommand(&cmd,&reply);
		if(status == ParseStatus::NOT_FOUND || status == ParseStatus::OK_CONTINUE){ // Not a system command
			// Call all command handlers
			for(CommandHandler* handler : cmdHandlers){
				if(handler->hasCommands()){
					ParseStatus newstatus = handler->command(&cmd,&reply);
					// If last class did not have commands but a previous one asked to continue keep continue status
					if(!(status == ParseStatus::OK_CONTINUE && newstatus == ParseStatus::NOT_FOUND)){
						status = newstatus;
					}
					if(status == ParseStatus::ERR || status == ParseStatus::OK)
						break; // Stop after this class if finished flag is returned
				}
			}
		}
		if(reply.empty() && status == ParseStatus::OK){
			reply = "OK";
		}
		// Append newline if reply is not empty
		if(!reply.empty() && reply.back()!='\n'){
			reply+='\n';
		}
		if(status == ParseStatus::NOT_FOUND){ //No class reported success. Show error
			reply = "Err(0). Unknown command\n";
		}else if(status == ParseStatus::ERR){ //Error reported in command
			reply += "Err(1). Execution error\n";
		}
		this->cmd_reply+=reply;
	}

	if(this->cmd_reply.length()>0){ // Check if cdc busy
		if(CDC_Transmit_FS(this->cmd_reply.c_str(), this->cmd_reply.length()) != USBD_OK){
			this->usb_busy_retry = true;
		}
	}
}

/*
 * Global callback if cdc transfer is finished. Used to retry a failed transfer
 */
void FFBoardMain::cdcFinished(){
	if(this->usb_busy_retry){
		if(CDC_Transmit_FS(this->cmd_reply.c_str(), this->cmd_reply.length()) == USBD_OK){
			this->usb_busy_retry = false;
		}
	}
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


FFBoardMain::~FFBoardMain() {

}

