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



FFBoardMainIdentifier FFBoardMain::info ={.name = "Basic" , .id=0};

FFBoardMain::FFBoardMain() {


}

const FFBoardMainIdentifier FFBoardMain::getInfo(){
	return info;
}




void FFBoardMain::cdcRcv(char* Buf, uint32_t *Len){
	if(this->parser.add(Buf, Len)){
		executeCommands(this->parser.parse());
	}
}

bool FFBoardMain::executeUserCommand(ParsedCommand *cmd,std::string* reply){

	return false;
}

bool FFBoardMain::executeSysCommand(ParsedCommand* cmd,std::string* reply){
	bool flag = true;
	if(cmd->cmd == "help"){
		*reply += parser.helpstring;
		*reply += "\nSystem Commands: reboot,help,swver (Version),lsconf (List configs),id,config (Set config),vint,vext,format (Erase flash)\n";
		flag = false; // Continue to user commands
	}else if(cmd->cmd == "reboot"){
		NVIC_SystemReset();

	}else if(cmd->cmd == "vint"){
		if(cmd->type==get){
			*reply+=std::to_string(getIntV());
		}

	}else if(cmd->cmd == "vext"){
		if(cmd->type==get){
			*reply+=std::to_string(getExtV());
		}

	}else if(cmd->cmd == "swver"){
		*reply += std::to_string(SW_VERSION);

	}else if(cmd->type!=set &&cmd->cmd == "lsconf"){
		*reply += printAvailableClasses();

	}else if(cmd->cmd == "id"){
		*reply+=std::to_string(this->getInfo().id);
	}else if(cmd->cmd == "config"){
		if(cmd->type == get || cmd->type == none){
			uint16_t buf=this->getInfo().id;
			Flash_Read(ADR_CURRENT_CONFIG, &buf);
			*reply+=std::to_string(buf);

		}else if(cmd->type == set){
			if(isValidClassId(cmd->val)){
				Flash_Write(ADR_CURRENT_CONFIG, (uint16_t)cmd->val);
				if(cmd->val != this->getInfo().id){
					NVIC_SystemReset(); // Reboot
				}
			}else if(cmd->type == err){
				*reply += "Err";
			}
		}
	}else if(cmd->cmd == "format"){
		if(cmd->type == set && cmd->val==1){
			EE_Format();
		}else{
			*reply += "format=1 will ERASE ALL stored variables. Be careful!";
		}
	}else{
		flag = false;
	}
	// Append newline if reply is not empty

	return flag;
}

void FFBoardMain::executeCommands(std::vector<ParsedCommand> commands){
	std::string reply;
	for(ParsedCommand cmd : commands){
		if(!executeSysCommand(&cmd,&reply)){
			executeUserCommand(&cmd,&reply);

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

void FFBoardMain::adcUpd(volatile uint32_t* ADC_BUF){

}

void FFBoardMain::exti(uint16_t GPIO_Pin){

}

void FFBoardMain::hidGet(uint8_t id,uint16_t len,uint8_t** return_buf){

}

void FFBoardMain::hidOut(uint8_t* report){

}

void FFBoardMain::timerElapsed(TIM_HandleTypeDef* htim){

}

void FFBoardMain::uartRcv(UART_HandleTypeDef *huart){
	// Example
//	extern UART_HandleTypeDef UART;
//  if(huart == &UART)....
//	if(HAL_UART_Receive_IT(huart,(uint8_t*)&uart_buf,1) != HAL_OK){
//
//	  	  return;
//	}
}

void FFBoardMain::SOF(){

}


FFBoardMain::~FFBoardMain() {
	// TODO Auto-generated destructor stub
}

