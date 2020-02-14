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
	AdcHandler();
	virtual ~AdcHandler();
	virtual void adcUpd(volatile uint32_t* ADC_BUF);
};

#endif /* ADCHANDLER_H_ */
