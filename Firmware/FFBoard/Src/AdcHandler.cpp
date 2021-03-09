/*
 * AdcHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "AdcHandler.h"

std::vector<AdcHandler*> AdcHandler::adcHandlers;

AdcHandler::AdcHandler() {
	addCallbackHandler(adcHandlers, this);

}

AdcHandler::~AdcHandler() {
	removeCallbackHandler(adcHandlers, this);
}

// ADC updated
void AdcHandler::adcUpd(volatile uint32_t* ADC_BUF, uint8_t chans, ADC_HandleTypeDef* hadc){

}
