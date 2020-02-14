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
	this->spi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	this->spi->Init.FirstBit = SPI_FIRSTBIT_LSB;
	this->spi->Init.CLKPhase = SPI_PHASE_1EDGE;
	this->spi->Init.CLKPolarity = SPI_POLARITY_LOW;
	HAL_SPI_Init(this->spi);
}

SPI_Buttons::~SPI_Buttons() {

}


void SPI_Buttons::readButtons(uint8_t* buf ,uint16_t len){
	len = MIN(len,buttons);
	len = 1+((len-1)/8);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_Receive(this->spi,buf,len,5);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);

	if(this->invert){
		for(uint8_t i=0;i<len;i++){
			buf[i] = ~(buf[i]);
		}
	}
}

void SPI_Buttons::setInvert(bool invert){
	this->invert = invert;
}


uint16_t SPI_Buttons::getBtnNum(){
	return buttons;
}

void SPI_Buttons::setBtnNum(uint16_t num){
	buttons = num;
}
