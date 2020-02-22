/*
 * ButtonSourceSPI.cpp
 *
 *  Created on: 11.02.2020
 *      Author: Yannick
 */

#include "constants.h"
#include <SPIButtons.h>
#include "eeprom_addresses.h"
#include "flash_helpers.h"

ClassIdentifier SPI_Buttons::info = {
		 .name = "SPI" ,
		 .id=1
 };

SPI_Buttons::SPI_Buttons() {
	uint16_t confint = 0;
	Flash_Read(ADR_SPI_BTN_CONF, &confint);
	this->setConfig(decodeIntToConf(confint));
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

// Overload and save to flash
void SPI_Buttons::setConfig(ButtonSourceConfig config){
	ButtonSource::setConfig(config);
	Flash_Write(ADR_SPI_BTN_CONF, encodeConfToInt(config));
}

const ClassIdentifier SPI_Buttons::getInfo(){
	return info;
}


// Static flash helpers
void SPI_Buttons::writeConfNumButtons(int16_t num){
	uint16_t confint = 0;
	Flash_Read(ADR_SPI_BTN_CONF, &confint);
	ButtonSourceConfig conf = ButtonSource::decodeIntToConf(confint);

	if(num < 0){
		num = -num;
		conf.cutRight = true;
	}else{
		conf.cutRight = false;
	}
	num = MIN(num,32);

	conf.numButtons = num;
	confint = ButtonSource::encodeConfToInt(conf);
	Flash_Write(ADR_SPI_BTN_CONF, confint);
}

int16_t SPI_Buttons::readConfNumButtons(){
	uint16_t confint = 0;
	Flash_Read(ADR_SPI_BTN_CONF, &confint);
	ButtonSourceConfig conf = ButtonSource::decodeIntToConf(confint);

	if(conf.cutRight)
		return -conf.numButtons;
	else
		return conf.numButtons;
}



void SPI_Buttons::readButtons(uint32_t* buf){

	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_Receive(this->spi,(uint8_t*)buf,MIN(this->bytes,4),5);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);

	process(buf);
}


