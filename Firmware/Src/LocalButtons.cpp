/*
 * LocalButtons.cpp
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#include <LocalButtons.h>

ClassIdentifier LocalButtons::info = {
	 .name = "D-Pins" ,
	 .id=0
};

LocalButtons::LocalButtons() {
	setMask(mask); // Initialize button count
	restoreFlash();
}

LocalButtons::~LocalButtons() {
	// TODO Auto-generated destructor stub
}

const ClassIdentifier LocalButtons::getInfo(){
	return info;
}

void LocalButtons::setMask(uint32_t mask){
	this->mask = mask;

	this->btnnum = 0;
	for(uint8_t i = 0;i<this->maxButtons;i++){
		if(mask & (1 << i)){
			this->btnnum++;
		}
	}
}

void LocalButtons::readButtons(uint32_t* buf){
	uint8_t cur_btn = 0;
	for(uint8_t i = 0;i<this->maxButtons;i++){
		if(mask & (0x1 << i)){
			*buf |= !HAL_GPIO_ReadPin(button_ports[i],button_pins[i]) << cur_btn++;
		}
	}
}

void LocalButtons::saveFlash(){
	uint16_t dat = this->mask & 0xff;
	Flash_Write(ADR_LOCAL_BTN_CONF, dat);
}

void LocalButtons::restoreFlash(){
	uint16_t dat = 0;
	if(Flash_Read(ADR_LOCAL_BTN_CONF,&dat)){
		this->setMask(dat & 0xff);
	}
}

ParseStatus LocalButtons::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus result = ParseStatus::OK;
	// mask is a bitfield of 8 bits enabling or disabling specific pins
	if(cmd->cmd == "local_btnmask"){
		if(cmd->type == CMDtype::set){
			this->setMask(cmd->val);
		}else if(cmd->type == CMDtype::get){
			*reply += std::to_string(this->mask);
		}
	}else{
		result = ParseStatus::NOT_FOUND; // No valid command
	}

	return result;
}

