/*
 * LocalButtons.cpp
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#include <LocalButtons.h>

ClassIdentifier LocalButtons::info = {
	 .name = "D-Pins" ,
	 .id=CLSID_BTN_LOCAL,
};

LocalButtons::LocalButtons() : CommandHandler("dpin",CLSID_BTN_LOCAL,0) {
	setMask(mask); // Initialize button count
	restoreFlash();

	CommandHandler::registerCommands();
	registerCommand("mask", LocalButtons_commands::mask, "Enabled pins",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("polarity", LocalButtons_commands::polarity, "Pin polarity",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("pins", LocalButtons_commands::pins, "Available pins",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("values", LocalButtons_commands::values, "pin values",CMDFLAG_GET);
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

uint8_t LocalButtons::readButtons(uint64_t* buf){
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
	return this->btnnum;
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

CommandStatus LocalButtons::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<LocalButtons_commands>(cmd.cmdId)){
	case LocalButtons_commands::mask:
		if(cmd.type == CMDtype::set){
			this->setMask(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->mask));
		}else{
			return CommandStatus::ERR;
		}
	break;

	case LocalButtons_commands::polarity:
		if(cmd.type == CMDtype::set){
			this->polarity = cmd.val != 0;
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->polarity ? 1 : 0));
		}else{
			return CommandStatus::ERR;
		}
	break;

	case LocalButtons_commands::pins:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(maxButtons));
		}else{
			return CommandStatus::ERR;
		}
	break;

	case LocalButtons_commands::values:
		if(cmd.type == CMDtype::get){
			uint64_t buf = 0;
			readButtons(&buf);
			replies.push_back(CommandReply(buf));
		}else{
			return CommandStatus::ERR;
		}
	break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;

}

