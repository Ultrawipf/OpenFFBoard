/*
 * LocalAnalog.cpp
 *
 *  Created on: Nov 6, 2020
 *      Author: Yannick
 */

#include "LocalAnalog.h"
#include "global_callbacks.h"

ClassIdentifier LocalAnalog::info = {
	 .name = "AIN-Pins" ,
	 .id=0
};


LocalAnalog::LocalAnalog() {

}

LocalAnalog::~LocalAnalog() {

}

const ClassIdentifier LocalAnalog::getInfo(){
	return info;
}


//TODO auto ranging, settings

std::vector<uint32_t>* LocalAnalog::getAxes(){
	uint8_t chans = 0;
	this->buf.clear();
	volatile uint32_t* adcbuf = getAnalogBuffer(&AIN_HADC,&chans);

	uint8_t axes = MIN(chans-ADC_CHAN_FPIN,ADC_PINS);

	for(uint8_t i = 0;i<axes;i++){
		uint32_t val = adcbuf[ADC_CHAN_FPIN + i];
		this->buf.push_back(val);
	}
	return &this->buf;
}
