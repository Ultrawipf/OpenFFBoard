/*
 * ButtonSource.cpp
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#include "ButtonSource.h"

ClassIdentifier ButtonSource::info = {
	 .name 	= "NONE" ,
	 .id	= 0,
	 .hidden = true
};


ButtonSource::ButtonSource() {

}

ButtonSource::~ButtonSource() {

}


uint16_t ButtonSource::getBtnNum(){
	return this->btnnum;
}

