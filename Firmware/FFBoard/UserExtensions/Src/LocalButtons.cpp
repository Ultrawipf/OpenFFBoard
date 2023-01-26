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
	uint16_t dat = this->mask & 0xffff;
	Flash_Write(ADR_LOCAL_BTN_CONF, dat);

	uint16_t dat2 = this->polarity & 0x01;
	Flash_Write(ADR_LOCAL_BTN_CONF_2, dat2);
}

void LocalButtons::restoreFlash(){
	uint16_t dat = 0;
	if(Flash_Read(ADR_LOCAL_BTN_CONF,&dat)){
		this->setMask(dat & 0xffff);
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
			replies.emplace_back(this->mask);
		}else{
			return CommandStatus::ERR;
		}
	break;

	case LocalButtons_commands::polarity:
		if(cmd.type == CMDtype::set){
			this->polarity = cmd.val != 0;
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(this->polarity ? 1 : 0);
		}else{
			return CommandStatus::ERR;
		}
	break;

	case LocalButtons_commands::pins:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(maxButtons);
		}else{
			return CommandStatus::ERR;
		}
	break;

	case LocalButtons_commands::values:
		if(cmd.type == CMDtype::get){
			uint64_t buf = 0;
			readButtons(&buf);
			replies.emplace_back(buf);
		}else{
			return CommandStatus::ERR;
		}
	break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;

}

/**
 * All DIN pins must be defined as inputs.
 * Not more than 16 pins can be defined
 */
const std::array<InputPin,BUTTON_PINS> LocalButtons::button_pins {
#if BUTTON_PINS > 0
		InputPin(DIN0_GPIO_Port, DIN0_Pin),
#endif
#if BUTTON_PINS > 1
		InputPin(DIN1_GPIO_Port, DIN1_Pin),
#endif
#if BUTTON_PINS > 2
		InputPin(DIN2_GPIO_Port, DIN2_Pin),
#endif
#if BUTTON_PINS > 3
		InputPin(DIN3_GPIO_Port, DIN3_Pin),
#endif
#if BUTTON_PINS > 4
		InputPin(DIN4_GPIO_Port, DIN4_Pin),
#endif
#if BUTTON_PINS > 5
		InputPin(DIN5_GPIO_Port, DIN5_Pin),
#endif
#if BUTTON_PINS > 6
		InputPin(DIN6_GPIO_Port, DIN6_Pin),
#endif
#if BUTTON_PINS > 7
		InputPin(DIN7_GPIO_Port, DIN7_Pin),
#endif
#if BUTTON_PINS > 8
		InputPin(DIN8_GPIO_Port, DIN8_Pin),
#endif
#if BUTTON_PINS > 9
		InputPin(DIN9_GPIO_Port, DIN9_Pin),
#endif
#if BUTTON_PINS > 10
		InputPin(DIN10_GPIO_Port, DIN10_Pin),
#endif
#if BUTTON_PINS > 11
		InputPin(DIN11_GPIO_Port, DIN11_Pin),
#endif
#if BUTTON_PINS > 12
		InputPin(DIN12_GPIO_Port, DIN12_Pin),
#endif
#if BUTTON_PINS > 13
		InputPin(DIN13_GPIO_Port, DIN13_Pin),
#endif
#if BUTTON_PINS > 14
		InputPin(DIN14_GPIO_Port, DIN14_Pin),
#endif
#if BUTTON_PINS > 15
		InputPin(DIN15_GPIO_Port, DIN15_Pin),
#endif

};
