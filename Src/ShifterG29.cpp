/*
 * ShifterG29.cpp
 *
 *  Created on: 27.02.2020
 *      Author: Yannick
 */

#include "ShifterG29.h"

ClassIdentifier ShifterG29::info = {
		 .name = "Shifter G29" ,
		 .id=2
 };
const ClassIdentifier ShifterG29::getInfo(){
	return info;
}

ShifterG29::ShifterG29() {

}

ShifterG29::~ShifterG29() {

}



void ShifterG29::readButtons(uint32_t* buf){
	gear = 0;
	*buf = 0;

	if(conf.invert){ // Sequential mode
		if(x_val < X_12){
			gear = 1;
		}else if(x_val > X_56){
			gear = 2;
		}
	}else{
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

uint16_t ShifterG29::getBtnNum(){
	if(conf.invert){
		return 2;
	}else{
		return 7;
	}
}

void ShifterG29::adcUpd(volatile uint32_t* ADC_BUF){
	x_val = ADC_BUF[ADC_CHAN_FPIN+x_chan];
	y_val = ADC_BUF[ADC_CHAN_FPIN+y_chan];
}


void ShifterG29::saveFlash(){
	Flash_Write(ADR_SHIFTER_BTN_CONF, encodeConfToInt(conf));
}

void ShifterG29::restoreFlash(){
	uint16_t confint = 0;
	Flash_Read(ADR_SHIFTER_BTN_CONF, &confint);
	this->setConfig(decodeIntToConf(confint));
}
