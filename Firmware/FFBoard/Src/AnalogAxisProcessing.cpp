/*
 * AnalogAxisProcessing.cpp
 *
 *  Created on: Jun 29, 2022
 *      Author: Yannick
 */

#include "AnalogAxisProcessing.h"
#include "CommandHandler.h"

/**
 * Constructs an analog axis processing class
 * @param[in] axisAmount maximum amount of axes to be processed
 * @param[in] cmdHandler command handler to register commands to. nullptr to disable commands
 * @param[in] allowFilters enable the use of filters. Creates filters and activates filter command
 * @param[in] allowAutoscale enable autoscaling command
 * @param[in] allowRawvalues enable raw value command. Required if manual scale should be used with the GUI
 * @param[in] allowManualScale activates min/max commands if set
 */
AnalogAxisProcessing::AnalogAxisProcessing(const uint32_t axisAmount,AnalogSource* analogSource,CommandHandler* cmdHandler,bool allowFilters,bool allowAutoscale,bool allowRawValues,bool allowManualScale) : axisAmount(axisAmount),analogSource(analogSource), modes({allowFilters,allowAutoscale,allowRawValues,allowManualScale}){

	if(allowAutoscale || allowManualScale){
		minMaxVals.resize(axisAmount); // We need min/max pairs if autoscaling or manual scaling is allowed
	}

	if(allowFilters){
		filters = std::vector<Biquad>(axisAmount); // Allocate filter
		// Activate command
		if(cmdHandler)
			cmdHandler->registerCommand("filter", AnalogAxisProcessing_commands::filter, "Enable lowpass filters",CMDFLAG_GET|CMDFLAG_SET);

	}else{
		conf.filtersEnabled = false;
	}


	if(allowAutoscale){
		if(cmdHandler)
			cmdHandler->registerCommand("autocal", AnalogAxisProcessing_commands::autoscale, "Autoranging",CMDFLAG_GET|CMDFLAG_SET);
	}else{
		conf.autorange = false;
	}

	if(analogSource)
		cmdHandler->registerCommand("values", AnalogAxisProcessing_commands::values, "Analog output values",CMDFLAG_GET);

	if(allowRawValues)
		cmdHandler->registerCommand("rawval", AnalogAxisProcessing_commands::rawvalues, "All raw values",CMDFLAG_GET);

	if(allowManualScale && cmdHandler){
		cmdHandler->registerCommand("min", AnalogAxisProcessing_commands::min, "Min value limit (adr=chan)",CMDFLAG_GETADR|CMDFLAG_SETADR);
		cmdHandler->registerCommand("max", AnalogAxisProcessing_commands::max, "Max value limit (adr=chan)",CMDFLAG_GETADR|CMDFLAG_SETADR);
		conf.raw = false;
	}
	setupFilters(); // Initialize filters
}

void registerCommands(CommandHandler* cmdhandler){

}

AnalogAxisProcessing::~AnalogAxisProcessing() {

}


void AnalogAxisProcessing::setupFilters(){
	for(Biquad& filter : filters){
		filter.setBiquad(BiquadType::lowpass, filterF, filterQ, 0.0);
	}
	filterSamples = 0;
}

/**
 * Returns processing config. Use this to save settings
 */
AnalogProcessingConfig& AnalogAxisProcessing::getAnalogProcessingConfig(){
	return this->conf;
}

/**
 * Sets the processing config. Use this to restore settings
 */
void AnalogAxisProcessing::setAnalogProcessingConfig(AnalogProcessingConfig conf){
	this->conf = conf;
}

void AnalogAxisProcessing::processAxes(std::vector<int32_t>& buf){
	if(modes.allowRawvalues){
		this->rawValues = buf;
	}

	for(uint32_t i = 0 ; i < std::min<uint32_t>(this->axisAmount , buf.size()) ; i++){
		// Modify values in vector directly
		int32_t val = buf[i];
		// Apply filter
		if(conf.filtersEnabled && i < filters.size()){
			val = filters[i].process(val);
			if(filterSamples <= waitFilterSamples){
				filterSamples++;
			}
		}


		// Only process if minMaxVals are available
		if(i < minMaxVals.size()){

			// If filter and autoranging enabled wait for enough samples to update min/max
			if(conf.autorange && (filterSamples > waitFilterSamples || !conf.filtersEnabled)){
				minMaxVals[i].max = std::max(minMaxVals[i].max,val);
				minMaxVals[i].min = std::min(minMaxVals[i].min,val);
			}

			int32_t range = (minMaxVals[i].max - minMaxVals[i].min);
			if(range > 1 && range <= 0xffff && !conf.raw){
				float scaler = ((float)0xffff / (float)range)*(autorangeScale);
				val *= scaler;
				val = val - ((scaler*(float)minMaxVals[i].min) + 0x7fff);
			}
		}
		val = clip(val,-0x7fff,0x7fff); // Clip if slightly out of range because of inaccuracy
		buf[i] = val; // Store in buffer
	}

}


/**
 * To be called by the class using this. Not an actual command handler
 */
CommandStatus AnalogAxisProcessing::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<AnalogAxisProcessing_commands>(cmd.cmdId))
	{
	case AnalogAxisProcessing_commands::values:
		if(cmd.type == CMDtype::get){
			std::vector<int32_t>* axes = this->analogSource->getAxes();

			for(int32_t val : *axes){
				replies.emplace_back(val);
			}

		}else{
			return CommandStatus::ERR;
		}
		break;

	case AnalogAxisProcessing_commands::rawvalues:
		if(cmd.type == CMDtype::get){
			for(int32_t val : this->rawValues){
				replies.emplace_back(val);
			}

		}else{
			return CommandStatus::ERR;
		}
		break;

	case AnalogAxisProcessing_commands::autoscale:
		if( (cmd.type == CMDtype::set && cmd.val != 0) || !modes.allowManualScale){
			// Reset autorange
			for(uint8_t i = 0 ; i<minMaxVals.size() ; i++){
				minMaxVals[i] = MinMaxPair(); // Reset minmax
			}
		}
		return CommandHandler::handleGetSet(cmd, replies, this->conf.autorange);

	case AnalogAxisProcessing_commands::filter:
		if(cmd.type == CMDtype::set)
			setupFilters(); // Reset filters
		return CommandHandler::handleGetSet(cmd, replies, this->conf.filtersEnabled);

	case AnalogAxisProcessing_commands::max:
		// Valid if address is in pin range and value is 16b int
		if(cmd.adr >= 0 && cmd.adr <= axisAmount && cmd.val >= -0x7fff && cmd.val <= 0x7fff){
			if(cmd.type == CMDtype::getat){
				replies.emplace_back(minMaxVals[cmd.adr].max);
				break;
			}else if(cmd.type == CMDtype::setat){
				minMaxVals[cmd.adr].max = cmd.val;
				break;
			}
		}
		return CommandStatus::ERR; // Invalid

	case AnalogAxisProcessing_commands::min:
		// Valid if address is in pin range and value is 16b int
		if(cmd.adr >= 0 && cmd.adr <= axisAmount && cmd.val >= -0x7fff && cmd.val <= 0x7fff){
			if(cmd.type == CMDtype::getat){
				replies.emplace_back(minMaxVals[cmd.adr].min);
				break;
			}else if(cmd.type == CMDtype::setat){
				minMaxVals[cmd.adr].min = cmd.val;
				break;
			}
		}
		return CommandStatus::ERR; // Invalid

	default:
	return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}



/**
 * Config takes 2 lowest bits
 */
AnalogProcessingConfig AnalogAxisProcessing::decodeAnalogProcessingConfFromInt(uint16_t val){
	AnalogProcessingConfig aconf;
	aconf.autorange = (val) & 0x1;
	aconf.filtersEnabled = (val >> 1) & 0x1;
	return aconf;
}

/**
 * Config returns 2 lowest bits
 */
uint16_t AnalogAxisProcessing::encodeAnalogProcessingConfToInt(AnalogProcessingConfig& conf){
	uint16_t val = 0;
	val |= (conf.autorange & 0x1);
	val |= (conf.filtersEnabled & 0x1) << 1;
	return val;
}



