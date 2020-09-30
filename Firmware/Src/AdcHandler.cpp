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

void AdcHandler::adcUpd(volatile uint32_t* ADC_BUF){

}
