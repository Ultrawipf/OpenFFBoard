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

uint8_t AdcHandler::getAdcResolutionBits(ADC_HandleTypeDef* hadc){
#if defined(ADC_RESOLUTION_16B)
	if(hadc->Init.Resolution == ADC_RESOLUTION_16B) return 16;
#endif
#if defined(ADC_RESOLUTION_14B)
	if(hadc->Init.Resolution == ADC_RESOLUTION_14B) return 14;
#endif
#if defined(ADC_RESOLUTION_12B)
	if(hadc->Init.Resolution == ADC_RESOLUTION_12B) return 12;
#endif
#if defined(ADC_RESOLUTION_10B)
	if(hadc->Init.Resolution == ADC_RESOLUTION_10B) return 10;
#endif
#if defined(ADC_RESOLUTION_8B)
	if(hadc->Init.Resolution == ADC_RESOLUTION_8B) return 8;
#endif
#if defined(ADC_RESOLUTION_6B)
	if(hadc->Init.Resolution == ADC_RESOLUTION_6B) return 6;
#endif
	return 0; // Error
}

