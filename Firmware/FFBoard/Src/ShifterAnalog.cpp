/*
 * ShifterG29.cpp
 *
 *  Created on: 27.02.2020
 *      Author: Yannick
 */

#include <ShifterAnalog.h>
#include "global_callbacks.h"

ClassIdentifier ShifterAnalog::info = {
		 .name = "Shifter Analog" ,
		 .id=2
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

		if(gear == 6 && (HAL_GPIO_ReadPin(rev_port,rev_pin) == GPIO_PIN_SET)){
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



void ShifterAnalog::saveFlash(){
	uint16_t val = (uint8_t)this->mode;
	Flash_Write(ADR_SHIFTER_BTN_CONF, val);
}

void ShifterAnalog::restoreFlash(){
	uint16_t confint = 0;
	Flash_Read(ADR_SHIFTER_BTN_CONF, &confint);
	this->mode = ShifterMode(confint);
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
	}else{
		result = ParseStatus::NOT_FOUND;
	}
	return result;
}
