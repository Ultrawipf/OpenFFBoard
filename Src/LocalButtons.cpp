/*
 * LocalButtons.cpp
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#include <LocalButtons.h>

ClassIdentifier LocalButtons::info = {
	 .name = "D-Pins" ,
	 .id=0
};

LocalButtons::LocalButtons() {
	// Limit amount of buttons
	conf.numButtons = MIN(this->maxButtons, conf.numButtons);
}

LocalButtons::~LocalButtons() {
	// TODO Auto-generated destructor stub
}

const ClassIdentifier LocalButtons::getInfo(){
	return info;
}


void LocalButtons::readButtons(uint32_t* buf){
	uint8_t buttons = MIN(this->maxButtons, conf.numButtons);
	for(uint8_t i = 0;i<buttons;i++){
		*buf |= !HAL_GPIO_ReadPin(button_ports[i],button_pins[i]) << i;
	}

}

