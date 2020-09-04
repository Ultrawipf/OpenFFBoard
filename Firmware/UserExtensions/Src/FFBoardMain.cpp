/*
 * FFBoardMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include "FFBoardMain.h"

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
		executeCommands(this->parser.parse());
	}
}

bool FFBoardMain::command(ParsedCommand *cmd,std::string* reply){

	return false;
}

bool FFBoardMain::executeSysCommand(ParsedCommand* cmd,std::string* reply){
	bool flag = true;
	if(cmd->cmd == "help"){
		*reply += parser.helpstring;
		*reply += "\nSystem Commands: reboot,help,dfu,swver (Version),lsmain (List configs),id,main (Set main config),lsactive (print command handlers),vint,vext,format (Erase flash)\n";
		flag = false; // Continue to user commands
	}else if(cmd->cmd == "reboot"){
		NVIC_SystemReset();
	}else if(cmd->cmd == "dfu"){
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

	}else if(cmd->type!=CMDtype::set &&cmd->cmd == "lsmain"){
		*reply += mainchooser.printAvailableClasses();

	}else if(cmd->cmd == "id"){
		*reply+=std::to_string(this->getInfo().id);

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
				*reply += "Err";
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
		flag = false;
	}
	// Append newline if reply is not empty

	return flag;
}

/*
 * Executes parsed commands and calls other command handlers.
 * Not global so it can be overridden by main classes to change behaviour or suppress outputs.
 */
void FFBoardMain::executeCommands(std::vector<ParsedCommand> commands){
	std::string reply;
	extern std::vector<CommandHandler*> cmdHandlers;
	for(ParsedCommand cmd : commands){
		if(!executeSysCommand(&cmd,&reply)){
			// Call all command handlers
			for(CommandHandler* handler : cmdHandlers){
				if(handler->hasCommands())
					if(handler->command(&cmd,&reply))
						break; // Stop after this class if finished flag is returned
			}

		}
		if(!reply.empty() && reply.back()!='\n'){
			reply+='\n';
		}
	}
	if(reply.length()>0){
		CDC_Transmit_FS(reply.c_str(), reply.length());
	}
}

void FFBoardMain::usbInit(){

	USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
	USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
	USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
	USBD_Start(&hUsbDeviceFS);
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
	// TODO Auto-generated destructor stub
}

