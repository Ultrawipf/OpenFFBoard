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
			bool b{readButton(i)};
			if(this->polarity){ // positive polarity
				*buf |= b << cur_btn++;
			}else{	// Negative polarity (normal)
				*buf |= !b << cur_btn++;
			}
		}
	}
}

void LocalButtons::saveFlash(){
	uint16_t dat = this->mask & 0xff;
	Flash_Write(ADR_LOCAL_BTN_CONF, dat);

	uint16_t dat2 = this->polarity & 0x01;
	Flash_Write(ADR_LOCAL_BTN_CONF_2, dat2);
}

void LocalButtons::restoreFlash(){
	uint16_t dat = 0;
	if(Flash_Read(ADR_LOCAL_BTN_CONF,&dat)){
		this->setMask(dat & 0xff);
	}

	if(Flash_Read(ADR_LOCAL_BTN_CONF_2,&dat)){
		this->polarity = dat & 0x01;
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
		}else{
			result = ParseStatus::ERR;
		}
	}else if(cmd->cmd == "local_btnpol"){
		if(cmd->type == CMDtype::set){
			this->polarity = cmd->val != 0;
		}else if(cmd->type == CMDtype::get){
			*reply += std::to_string(this->polarity ? 1 : 0);
		}else{
			result = ParseStatus::ERR;
		}
	}else if(cmd->cmd == "local_btnpins"){
		if(cmd->type == CMDtype::get){
			*reply += std::to_string(maxButtons);
		}else{
			result = ParseStatus::ERR;
		}
	}else if(cmd->cmd == "help"){
		result = ParseStatus::OK_CONTINUE;
		*reply += "Digital pins: local_btnmask,local_btnpol,local_btnpins\n";
	}else{
		result = ParseStatus::NOT_FOUND; // No valid command
	}

	return result;
}

