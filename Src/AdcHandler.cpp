/*
 * AdcHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "AdcHandler.h"

AdcHandler::AdcHandler() {
	extern std::vector<AdcHandler*> adcHandlers;
	adcHandlers.push_back(this);

}

AdcHandler::~AdcHandler() {
	extern std::vector<AdcHandler*> adcHandlers;
	for (uint8_t i = 0; i < adcHandlers.size(); i++){
		if(adcHandlers[i] == this){
			adcHandlers.erase(adcHandlers.begin()+i);
			break;
		}
	}
}

void AdcHandler::adcUpd(volatile uint32_t* ADC_BUF){

}
