/*
 * LocalAnalog.cpp
 *
 *  Created on: Nov 6, 2020
 *      Author: Yannick
 */

#include "LocalAnalog.h"
#include "global_callbacks.h"
#include "flash_helpers.h"

ClassIdentifier LocalAnalog::info = {
	 .name = "AIN-Pins" ,
	 .id=0
};


LocalAnalog::LocalAnalog() {
	restoreFlash();
}

LocalAnalog::~LocalAnalog() {

}

const ClassIdentifier LocalAnalog::getInfo(){
	return info;
}

void LocalAnalog::saveFlash(){
	Flash_Write(ADR_LOCALANALOG_MASK, LocalAnalog::encodeAnalogConfToInt(this->aconf));
}

void LocalAnalog::restoreFlash(){
	uint16_t aconfint;
	if(Flash_Read(ADR_LOCALANALOG_MASK,&aconfint)){
		this->aconf = LocalAnalog::decodeAnalogConfFromInt(aconfint);
	}
}


//TODO auto ranging, settings

std::vector<int32_t>* LocalAnalog::getAxes(){
	uint8_t chans = 0;
	this->buf.clear();
	volatile uint32_t* adcbuf = getAnalogBuffer(&AIN_HADC,&chans);

	uint8_t axes = MIN(chans-ADC_CHAN_FPIN,ADC_PINS);

	for(uint8_t i = 0;i<axes;i++){
		if(!(aconf.analogmask & 0x01 << i))
			continue;

		int32_t val = ((adcbuf[i+ADC_CHAN_FPIN] & 0xFFF) << 4)-0x7fff;
		this->buf.push_back(val);
	}
	return &this->buf;
}

ParseStatus LocalAnalog::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus flag = ParseStatus::OK;
	if(cmd->cmd == "local_ain_mask"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(aconf.analogmask);
		}else if(cmd->type == CMDtype::set){
			aconf.analogmask = cmd->val;
		}else{
			flag = ParseStatus::ERR;
		}
	}else if(cmd->cmd == "local_ain_num"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(ADC_PINS); // Max num inputs
		}else{
			flag = ParseStatus::ERR;
		}
	}else if(cmd->cmd == "help"){
		flag = ParseStatus::OK_CONTINUE;
		*reply += "Analog pins: local_ain_mask,local_ain_num\n";
	}else{
		flag = ParseStatus::NOT_FOUND;
	}
	return flag;
}


FFBWheelAnalogConfig LocalAnalog::decodeAnalogConfFromInt(uint16_t val){
	FFBWheelAnalogConfig aconf;
	aconf.analogmask = val & 0xff;
	aconf.invertX = (val >> 8) & 0x1;
	return aconf;
}
uint16_t LocalAnalog::encodeAnalogConfToInt(FFBWheelAnalogConfig conf){
	uint16_t val = conf.analogmask & 0xff;
	val |= (conf.invertX & 0x1) << 8;
	return val;
}
