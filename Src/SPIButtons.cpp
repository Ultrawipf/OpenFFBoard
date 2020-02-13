/*
 * ButtonSourceSPI.cpp
 *
 *  Created on: 11.02.2020
 *      Author: Yannick
 */

#include "constants.h"
#include <SPIButtons.h>

SPI_Buttons::SPI_Buttons() {
	spi = &HSPI2;
	cspin = SPI2_NSS_Pin;
	csport = SPI2_NSS_GPIO_Port;
}

SPI_Buttons::~SPI_Buttons() {

}


void SPI_Buttons::readButtons(uint8_t* buf ,uint16_t len){
	len = MIN(len,buttons);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_Receive(this->spi,buf,len,5);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);

}


uint16_t SPI_Buttons::getBtnNum(){
	return buttons;
}

void SPI_Buttons::setBtnNum(uint16_t num){
	buttons = num;
}
