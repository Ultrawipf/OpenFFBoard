/*
 * LocalAnalog.cpp
 *
 *  Created on: Nov 6, 2020
 *      Author: Yannick
 */

#include "LocalAnalog.h"
#include "global_callbacks.h"
#include "flash_helpers.h"
#include "eeprom_addresses.h"

ClassIdentifier LocalAnalog::info = {
	 .name = "AIN-Pins" ,
	 .id=CLSID_ANALOG_LOCAL, //0
};


const std::array<std::pair<uint16_t,uint16_t>,8> minMaxValAddr = {
	std::pair<uint16_t,uint16_t>(ADR_LOCALANALOG_MIN_0,ADR_LOCALANALOG_MAX_0),
	std::pair<uint16_t,uint16_t>(ADR_LOCALANALOG_MIN_1,ADR_LOCALANALOG_MAX_1),
	std::pair<uint16_t,uint16_t>(ADR_LOCALANALOG_MIN_2,ADR_LOCALANALOG_MAX_2),
	std::pair<uint16_t,uint16_t>(ADR_LOCALANALOG_MIN_3,ADR_LOCALANALOG_MAX_3),
	std::pair<uint16_t,uint16_t>(ADR_LOCALANALOG_MIN_4,ADR_LOCALANALOG_MAX_4),
	std::pair<uint16_t,uint16_t>(ADR_LOCALANALOG_MIN_5,ADR_LOCALANALOG_MAX_5),
	std::pair<uint16_t,uint16_t>(ADR_LOCALANALOG_MIN_6,ADR_LOCALANALOG_MAX_6),
	std::pair<uint16_t,uint16_t>(ADR_LOCALANALOG_MIN_7,ADR_LOCALANALOG_MAX_7),
};

LocalAnalog::LocalAnalog() : CommandHandler("apin",CLSID_ANALOG_LOCAL,0),AnalogAxisProcessing(ADC_PINS,this,this, true, true,false) {
	this->restoreFlash();

	CommandHandler::registerCommands();
	registerCommand("mask", LocalAnalog_commands::pinmask, "Enabled pins",CMDFLAG_GET|CMDFLAG_SET);

	registerCommand("pins", LocalAnalog_commands::pins, "Available pins",CMDFLAG_GET|CMDFLAG_SET);
	//registerCommand("values", LocalAnalog_commands::values, "Analog values",CMDFLAG_GET);

}

LocalAnalog::~LocalAnalog() {

}

const ClassIdentifier LocalAnalog::getInfo(){
	return info;
}

void LocalAnalog::saveFlash(){
	uint16_t processingConf = AnalogAxisProcessing::encodeAnalogProcessingConfToInt(AnalogAxisProcessing::getAnalogProcessingConfig());
	uint16_t conf1 = aconf.analogmask | (processingConf << 8);
	Flash_Write(ADR_LOCALANALOG_MASK, conf1);

	AnalogAxisProcessing::saveMinMaxValues(minMaxValAddr);
}

void LocalAnalog::restoreFlash(){
	uint16_t aconfint;
	if(Flash_Read(ADR_LOCALANALOG_MASK,&aconfint)){
		AnalogAxisProcessing::setAnalogProcessingConfig(AnalogAxisProcessing::decodeAnalogProcessingConfFromInt(aconfint >> 8));
		aconf.analogmask = aconfint & 0xff;
	}
	AnalogAxisProcessing::restoreMinMaxValues(minMaxValAddr);
}



std::vector<int32_t>* LocalAnalog::getAxes(){
	uint8_t chans = 0;
	this->buf.clear();
	volatile uint32_t* adcbuf = getAnalogBuffer(&AIN_HADC,&chans);

	uint8_t axes = std::min<uint8_t>(chans-ADC_CHAN_FPIN,numPins);

	for(uint8_t i = 0;i<axes;i++){
		if(!(aconf.analogmask & 0x01 << i))
			continue;

		int32_t val = ((adcbuf[i+ADC_CHAN_FPIN] & 0xFFF) << 4)-0x7fff;

		this->buf.push_back(val);
	}
	AnalogAxisProcessing::processAxes(buf);
	return &this->buf;
}


CommandStatus LocalAnalog::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<LocalAnalog_commands>(cmd.cmdId)){
		case LocalAnalog_commands::pinmask:
			return handleGetSet(cmd, replies, this->aconf.analogmask);
		break;

		case LocalAnalog_commands::pins:
			if(cmd.type == CMDtype::get){
				replies.emplace_back(numPins);
			}else{
				return CommandStatus::ERR;
			}
			break;
//		case LocalAnalog_commands::values:
//			if(cmd.type == CMDtype::get){
//				std::vector<int32_t>* axes = getAxes();
//
//				for(int32_t val : *axes){
//					replies.emplace_back(val);
//				}
//
//			}else{
//				return CommandStatus::ERR;
//			}
//			break;

		default:
			return AnalogAxisProcessing::command(cmd, replies); // Try processing command
		}

		return CommandStatus::OK;
}
