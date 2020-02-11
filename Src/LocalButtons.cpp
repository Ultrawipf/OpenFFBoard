/*
 * LocalButtons.cpp
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#include <LocalButtons.h>

LocalButtons::LocalButtons() {
	// TODO Auto-generated constructor stub

}

LocalButtons::~LocalButtons() {
	// TODO Auto-generated destructor stub
}

void LocalButtons::readButtons(uint8_t* buf ,uint16_t len){
	len = MIN(len,8);

	for(uint8_t i = 0;i<len;i++){
		*buf |= !HAL_GPIO_ReadPin(button_ports[i],button_pins[i]) << i;
	}

}

uint16_t LocalButtons::getBtnNum(){
	return 8;
}
