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

ButtonSourceConfig ButtonSource::getConfig(){
	return this->conf;
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

ButtonSourceConfig ButtonSource::decodeIntToConf(uint16_t val){
	ButtonSourceConfig conf;
	conf.numButtons = val & 0xff;
	conf.invert = (val >> 8) & 0x1;
	conf.cutRight = (val >> 9) & 0x1;
	return conf;
}
uint16_t ButtonSource::encodeConfToInt(ButtonSourceConfig conf){
	uint16_t val = conf.numButtons & 0xff;
	val |= conf.invert << 8;
	val |= conf.cutRight << 9;
	return val;
}


