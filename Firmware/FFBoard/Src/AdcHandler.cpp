/*
 * AdcHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "AdcHandler.h"

AdcHandler::AdcHandler() {
	extern std::vector<AdcHandler*> adcHandlers;
	addCallbackHandler(&adcHandlers, this);

}

AdcHandler::~AdcHandler() {
	extern std::vector<AdcHandler*> adcHandlers;
	removeCallbackHandler(&adcHandlers, this);
}

// ADC updated
void AdcHandler::adcUpd(volatile uint32_t* ADC_BUF, uint8_t chans, ADC_HandleTypeDef* hadc){

}
