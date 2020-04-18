/*
 * ButtonSource.cpp
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#include "ButtonSource.h"
#include "math.h"

ClassIdentifier ButtonSource::info = {
	 .name 	= "NONE" ,
	 .id	= 1337,
	 .hidden = true
};

ButtonSource::ButtonSource() {
	// TODO Auto-generated constructor stub

}

ButtonSource::~ButtonSource() {
	// TODO Auto-generated destructor stub
}


uint16_t ButtonSource::getBtnNum(){
	return this->conf.numButtons;
}

void ButtonSource::setConfig(ButtonSourceConfig config){
	config.numButtons = MIN(this->maxButtons, config.numButtons);
	this->conf = config;
	mask = pow(2,config.numButtons)-1;
	offset = 8 - (config.numButtons % 8);
	bytes = 1+((config.numButtons-1)/8);
}

ButtonSourceConfig* ButtonSource::getConfig(){
	return &this->conf;
}


void ButtonSource::process(uint32_t* buf){

	if(offset){
		if(this->conf.cutRight){
			*buf = *buf >> offset;
		}else{
			*buf = *buf & this->mask;
		}
	}
	if(conf.invert)
		*buf = (~*buf);

	*buf = *buf  & mask;
}

bool ButtonSource::command(ParsedCommand* cmd,std::string* reply){
	bool result = true;
	if(cmd->adr != this->getInfo().id){
		return false;
	}
	if(cmd->cmd == "btnnum"){
		if(cmd->type == CMDtype::setat){
			ButtonSourceConfig* c = this->getConfig();
			c->numButtons = cmd->val;
			this->setConfig(*c);

		}else if(cmd->type == CMDtype::getat){
			*reply+=std::to_string(this->getBtnNum());
		}else{
			*reply+="Err. Supply number of buttons for button source id as <c>?<id>=<num>";
		}
	}else if(cmd->cmd == "btnpol"){
		if(cmd->type == CMDtype::setat){
			ButtonSourceConfig* c = this->getConfig();
			c->invert = cmd->val == 0 ? false : true;
			this->setConfig(*c);

		}else if(cmd->type == CMDtype::getat){
			ButtonSourceConfig* c = this->getConfig();
			*reply+=std::to_string(c->invert);
		}else{
			*reply+="Err. invert: 1 else 0 for button source id as <c>?<id>=<pol>";
		}
	}else if(cmd->cmd == "btncut"){
		if(cmd->type == CMDtype::setat){
			ButtonSourceConfig* c = this->getConfig();
			c->cutRight = cmd->val == 0 ? false : true;
			this->setConfig(*c);

		}else if(cmd->type == CMDtype::getat){
			ButtonSourceConfig* c = this->getConfig();
			*reply+=std::to_string(c->cutRight);

		}else{
			*reply+="Err. cut right: 1 else 0 for button source id as <c>?<id>=<cut>";
		}
	}else{
		result = false;
	}
	return result;
}

ButtonSourceConfig ButtonSource::decodeIntToConf(uint16_t val){
	ButtonSourceConfig c;
	c.numButtons = val & 0x3F;
	c.invert = (val >> 6) & 0x1;
	c.cutRight = (val >> 7) & 0x1;
	c.extraOptions = (val >> 8);
	return c;
}
uint16_t ButtonSource::encodeConfToInt(ButtonSourceConfig* c){
	uint16_t val = c->numButtons & 0x3F;
	val |= c->invert << 6;
	val |= c->cutRight << 7;
	val |= c->extraOptions << 8;
	return val;
}


