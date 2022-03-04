/*
 * AnalogSource.cpp
 *
 *  Created on: 06.11.2020
 *      Author: Yannick
 */

#include "AnalogSource.h"

ClassIdentifier AnalogSource::info = {
	 .name 	= "NONE" ,
	 .id	= CLSID_ANALOG_NONE, //0
	 .visibility = ClassVisibility::hidden
};

AnalogSource::AnalogSource() {

}

AnalogSource::~AnalogSource() {

}

std::vector<int32_t>* AnalogSource::getAxes(){
	return &this->buf;
}
