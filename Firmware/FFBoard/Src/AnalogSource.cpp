/*
 * AnalogSource.cpp
 *
 *  Created on: 06.11.2020
 *      Author: Yannick
 */

#include "AnalogSource.h"

ClassIdentifier AnalogSource::info = {
	 .name 	= "NONE" ,
	 .id	= 0,
	 .hidden = true
};

AnalogSource::AnalogSource() {
	// TODO Auto-generated constructor stub

}

AnalogSource::~AnalogSource() {
	// TODO Auto-generated destructor stub
}

std::vector<uint32_t>* AnalogSource::getAxes(){
	return &this->buf;
}
