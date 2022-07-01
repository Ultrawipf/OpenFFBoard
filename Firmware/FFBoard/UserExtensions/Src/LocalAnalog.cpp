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
	 .id=CLSID_ANALOG_LOCAL, //0
};


std::vector<std::pair<uint16_t,uint16_t>> LocalAnalog::minMaxValAddr = {

};

LocalAnalog::LocalAnalog() : CommandHandler("apin",CLSID_ANALOG_LOCAL,0),AnalogAxisProcessing(ADC_PINS, this, true, true,std::nullopt) {
	this->restoreFlash();

	CommandHandler::registerCommands();
	registerCommand("mask", LocalAnalog_commands::pinmask, "Enabled pins",CMDFLAG_GET|CMDFLAG_SET);
//	registerCommand("autocal", LocalAnalog_commands::autocal, "Autoranging",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("pins", LocalAnalog_commands::pins, "Available pins",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("values", LocalAnalog_commands::values, "Analog values",CMDFLAG_GET);
//	registerCommand("filter", LocalAnalog_commands::filter, "Enable lowpass filter",CMDFLAG_GET|CMDFLAG_SET);
//	registerCommand("min", LocalAnalog_commands::min, "Min value limit",CMDFLAG_GETADR|CMDFLAG_SETADR);
//	registerCommand("max", LocalAnalog_commands::max, "Max value limit",CMDFLAG_GETADR|CMDFLAG_SETADR);
	setupFilters();
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


void LocalAnalog::setAutorange(bool autorange){
	for(uint8_t i = 0 ; i<numPins ; i++){
		minMaxVals[i] = MinMaxPair(); // Reset minmax
	}
	this->aconf.autorange = autorange;
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
//		// Filter before autoranging
//		if(aconf.filtersEnabled){
//			val = filters[i].process(val);
//			if(filterSamples <= waitFilterSamples){
//				filterSamples++;
//			}
//		}
//
//		if(aconf.autorange && (filterSamples > waitFilterSamples || !aconf.filtersEnabled)){
//			minMaxVals[i].max = std::max(minMaxVals[i].max,val);
//			minMaxVals[i].min = std::min(minMaxVals[i].min,val);
//		}
//		int32_t range = (minMaxVals[i].max - minMaxVals[i].min);
//		if(range > 1 && range <= 0xffff){
//			float scaler = ((float)0xffff / (float)range)*(autorangeScale);
//			val *= scaler;
//			val = val - ((scaler*(float)minMaxVals[i].min) + 0x7fff);
//		}
//		val = clip(val,-0x7fff,0x7fff); // Clip if slightly out of range because of inaccuracy

		this->buf.push_back(val);
	}
	AnalogAxisProcessing::processAxes(buf);
	return &this->buf;
}

/**
 * Calculates and resets filters
 */
void LocalAnalog::setupFilters(){
	for(Biquad& filter : filters){
		filter.setBiquad(BiquadType::lowpass, filterF, filterQ, 0.0);
	}
	filterSamples = 0;
}

CommandStatus LocalAnalog::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<LocalAnalog_commands>(cmd.cmdId)){
		case LocalAnalog_commands::pinmask:
			return handleGetSet(cmd, replies, this->aconf.analogmask);
		break;

		case LocalAnalog_commands::autocal:
			if(cmd.type == CMDtype::get){
				replies.emplace_back(aconf.autorange);
			}else if(cmd.type == CMDtype::set){
				setAutorange(cmd.val != 0);
			}
			break;
		case LocalAnalog_commands::pins:
			if(cmd.type == CMDtype::get){
				replies.emplace_back(numPins);
			}else{
				return CommandStatus::ERR;
			}
			break;
		case LocalAnalog_commands::values:
			if(cmd.type == CMDtype::get){
				std::vector<int32_t>* axes = getAxes();

				for(int32_t val : *axes){
					replies.emplace_back(val);
				}

			}else{
				return CommandStatus::ERR;
			}
			break;
		case LocalAnalog_commands::filter:
			setupFilters();
			return handleGetSet(cmd, replies, this->aconf.filtersEnabled);
		break;

		case LocalAnalog_commands::min:
			// Valid if address is in pin range and value is 16b int
			if(cmd.adr >= 0 && cmd.adr <= numPins && cmd.val >= -0x7fff && cmd.val <= 0x7fff){
				if(cmd.type == CMDtype::getat){
					replies.emplace_back(minMaxVals[cmd.adr].min);
					break;
				}else if(cmd.type == CMDtype::setat){
					minMaxVals[cmd.adr].min = cmd.val;
					break;
				}
			}
			return CommandStatus::ERR; // Invalid

		case LocalAnalog_commands::max:
			// Valid if address is in pin range and value is 16b int
			if(cmd.adr >= 0 && cmd.adr <= numPins && cmd.val >= -0x7fff && cmd.val <= 0x7fff){
				if(cmd.type == CMDtype::getat){
					replies.emplace_back(minMaxVals[cmd.adr].max);
					break;
				}else if(cmd.type == CMDtype::setat){
					minMaxVals[cmd.adr].max = cmd.val;
					break;
				}
			}
			return CommandStatus::ERR; // Invalid

		default:
			return AnalogAxisProcessing::command(cmd, replies); // Try processing command
		}

		return CommandStatus::OK;
}


LocalAnalogConfig LocalAnalog::decodeAnalogConfFromInt(uint16_t val){
	LocalAnalogConfig aconf;
	aconf.analogmask = val & 0xff;
	aconf.autorange = (val >> 8) & 0x1;
	aconf.filtersEnabled = (val >> 9) & 0x1;
	return aconf;
}
uint16_t LocalAnalog::encodeAnalogConfToInt(LocalAnalogConfig conf){
	uint16_t val = conf.analogmask & 0xff;
	val |= (conf.autorange & 0x1) << 8;
	val |= (conf.filtersEnabled & 0x1) << 9;
	return val;
}
