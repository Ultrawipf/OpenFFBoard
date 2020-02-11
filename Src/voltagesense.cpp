/*
 * voltagesense.cpp
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#include "global_callbacks.h"
#include "constants.h"
#include "voltagesense.h"

extern uint32_t ADC_BUF[ADC_CHANNELS];

uint16_t getIntV(){
	return ADC_BUF[ADC_CHAN_VINT] * VOLTAGE_MULT;
}

uint16_t getExtV(){
	return ADC_BUF[ADC_CHAN_VEXT] * VOLTAGE_MULT;
}
