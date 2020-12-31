/*
 * ShifterAnalog.cpp
 *
 *  Created on: 27.02.2020
 *      Author: Yannick
 */

#include <functional>

#include "ShifterAnalog.h"
#include "global_callbacks.h"

ClassIdentifier ShifterAnalog::info = {
		 .name = "Shifter Analog" ,
		 .id=3
 };
const ClassIdentifier ShifterAnalog::getInfo(){
	return info;
}

ShifterAnalog::ShifterAnalog() {
	this->restoreFlash();
}

ShifterAnalog::~ShifterAnalog() {

}

void ShifterAnalog::updateAdc(){
	uint8_t chans = 0;
	volatile uint32_t* buf = getAnalogBuffer(&AIN_HADC,&chans);
	x_val = buf[ADC_CHAN_FPIN+x_chan];
	y_val = buf[ADC_CHAN_FPIN+y_chan];
}

void ShifterAnalog::readButtons(uint32_t* buf){
	gear = 0;
	*buf = 0;
	updateAdc();
	if(mode == ShifterMode::G29_seq){ // Sequential mode
		if(x_val < X_12){
			gear = 1;
		}else if(x_val > X_56){
			gear = 2;
		}
	}else if(mode == ShifterMode::G29_H){
		// Calculate h shifter gears by thresholds
		if(x_val < X_12){
		  if(y_val > Y_135)
			  gear=1;       // 1st gear

		  if(y_val < Y_246)
			  gear=2;       // 2nd gear

		}else if(x_val > X_56){
		  if(y_val > Y_135)
			  gear=5;       // 5th gear

		  if(y_val < Y_246)
			  gear=6;       // 6th gear

		}else{
		  if(y_val > Y_135)
			  gear=3;       // 3rd gear

		  if(y_val < Y_246)
			  gear=4;       // 4th gear
		}

		if(gear == 6 && reverseButtonState){
			gear = 7; // Reverse
		}
	}

	if(gear > 0){
		*buf = 1 << (gear-1);
	}
}

uint16_t ShifterAnalog::getBtnNum(){
	if(mode == ShifterMode::G29_seq){
		return 2;
	}else if(mode == ShifterMode::G29_H){
		return 7;
	}else{
		return 0;
	}
}

void ShifterAnalog::updateButtonState(uint32_t* buf, uint16_t numButtons) {
	reverseButtonState = getButtonState(buf, numButtons, reverseButtonNum);
}

void ShifterAnalog::saveFlash(){
	Flash_Write(ADR_SHIFTER_BTN_CONF, static_cast<uint8_t>(mode));
	Flash_Write(ADR_SHIFTERANALOG_X_12, X_12);
	Flash_Write(ADR_SHIFTERANALOG_X_56, X_56);
	Flash_Write(ADR_SHIFTERANALOG_Y_135, Y_135);
	Flash_Write(ADR_SHIFTERANALOG_Y_246, Y_246);
	Flash_Write(ADR_SHIFTERANALOG_REV_BTN, reverseButtonNum);
}

void ShifterAnalog::restoreFlash(){
	mode = Flash_Read(ADR_SHIFTER_BTN_CONF, ShifterMode::G29_H);
	X_12 = Flash_Read(ADR_SHIFTERANALOG_X_12, X_12);
	X_56 = Flash_Read(ADR_SHIFTERANALOG_X_56, X_56);
	Y_135 = Flash_Read(ADR_SHIFTERANALOG_Y_135, Y_135);
	Y_246 = Flash_Read(ADR_SHIFTERANALOG_Y_246, Y_246);
	reverseButtonNum = Flash_Read(ADR_SHIFTERANALOG_REV_BTN, reverseButtonNum);
}

void ShifterAnalog::printModes(std::string* reply){
	for(uint8_t i = 0; i<mode_names.size();i++){
		*reply+=  mode_names[i]  + ":" + std::to_string(i)+"\n";
	}
}

ParseStatus ShifterAnalog::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus result = ParseStatus::OK;
	// use "shifter_mode!" to print a list of possible modes and choose one
	if(cmd->cmd == "shifter_mode"){
		if(cmd->type == CMDtype::set){
			this->mode = (ShifterMode)cmd->val;
		}else if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint8_t)this->mode);
		}else{
			printModes(reply);
		}
	}else if (cmd->cmd == "shifter_x_12"){
		handleGetSet(cmd, reply, X_12);
	}else if (cmd->cmd == "shifter_x_56"){
		handleGetSet(cmd, reply, X_56);
	}else if(cmd->cmd == "shifter_y_135"){
		handleGetSet(cmd, reply, Y_135);
	}else if(cmd->cmd == "shifter_y_246"){
		handleGetSet(cmd, reply, Y_246);
	}else if (cmd->cmd == "shifter_rev_btn"){
		handleGetSet(cmd, reply, reverseButtonNum);
	}else if (cmd->cmd == "shifter_vals" && cmd->type == CMDtype::get){
		*reply += std::to_string(x_val) + "," + std::to_string(y_val);
	}else{
		result = ParseStatus::NOT_FOUND;
	}
	return result;
}
