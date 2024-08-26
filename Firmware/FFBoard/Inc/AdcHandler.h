/*
 * AdcHandler.h
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#ifndef ADCHANDLER_H_
#define ADCHANDLER_H_

#include "cppmain.h"
#include "global_callbacks.h"

class AdcHandler {
public:
	static std::vector<AdcHandler*> adcHandlers;

	AdcHandler();
	virtual ~AdcHandler();
	virtual void adcUpd(volatile uint32_t* ADC_BUF, uint8_t chans, ADC_HandleTypeDef* hadc);

	static uint8_t getAdcResolutionBits(ADC_HandleTypeDef* hadc);
};

#endif /* ADCHANDLER_H_ */
