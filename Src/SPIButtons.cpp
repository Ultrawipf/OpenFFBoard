/*
 * ButtonSourceSPI.cpp
 *
 *  Created on: 11.02.2020
 *      Author: Yannick
 */

#include "constants.h"
#include <SPIButtons.h>
#include "eeprom_addresses.h"

ClassIdentifier SPI_Buttons::info = {
		 .name = "SPI" ,
		 .id=1
 };
const ClassIdentifier SPI_Buttons::getInfo(){
	return info;
}

SPI_Buttons::SPI_Buttons() {
	restoreFlash();
	this->conf.invert = true;
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

void SPI_Buttons::saveFlash(){
	Flash_Write(ADR_SPI_BTN_CONF, encodeConfToInt(conf));
}

void SPI_Buttons::restoreFlash(){
	uint16_t confint = 0;
	Flash_Read(ADR_SPI_BTN_CONF, &confint);
	this->setConfig(decodeIntToConf(confint));
}


void SPI_Buttons::readButtons(uint32_t* buf){

	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_Receive(this->spi,(uint8_t*)buf,MIN(this->bytes,4),5);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);

	process(buf);
}


