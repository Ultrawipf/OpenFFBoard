/*
 * ButtonSourceSPI.cpp
 *
 *  Created on: 11.02.2020
 *      Author: Yannick
 */

#include "constants.h"
#include <SPIButtons.h>
#include "eeprom_addresses.h"
#include "math.h"

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
	this->setCommandsEnabled(true);
}

SPI_Buttons::~SPI_Buttons() {

}
void SPI_Buttons::setConfig(ButtonSourceConfig config){
	config.numButtons = MIN(this->maxButtons, config.numButtons);
	this->conf = config;
	mask = pow(2,config.numButtons)-1;
	offset = 8 - (config.numButtons % 8);
	bytes = 1+((config.numButtons-1)/8);
	this->btnnum = config.numButtons;
}

ButtonSourceConfig* SPI_Buttons::getConfig(){
	return &this->conf;
}

void SPI_Buttons::saveFlash(){
	Flash_Write(ADR_SPI_BTN_CONF, SPI_Buttons::encodeConfToInt(this->getConfig()));
}

void SPI_Buttons::restoreFlash(){
	uint16_t confint = 0;
	Flash_Read(ADR_SPI_BTN_CONF, &confint);
	this->setConfig(SPI_Buttons::decodeIntToConf(confint));
}

void SPI_Buttons::process(uint32_t* buf){
	if(offset){
		if(this->conf.cutRight){
			*buf = *buf >> offset;
		}else{
			*buf = *buf & this->mask;
		}
	}
	if(conf.invert)
		*buf = (~*buf);
	*buf = *buf  & mask;
}
void SPI_Buttons::readButtons(uint32_t* buf){

	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_Receive(this->spi,(uint8_t*)buf,MIN(this->bytes,4),5);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);

	process(buf);
}

bool SPI_Buttons::command(ParsedCommand* cmd,std::string* reply){
	bool result = true;
	if(cmd->cmd == "spi_btnnum"){
		if(cmd->type == CMDtype::set){
			ButtonSourceConfig* c = this->getConfig();
			c->numButtons = cmd->val;
			this->setConfig(*c);

		}else if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->getBtnNum());
		}else{
			*reply+="Err. Supply number of buttons";
		}
	}else if(cmd->cmd == "spi_btnpol"){
		if(cmd->type == CMDtype::set){
			ButtonSourceConfig* c = this->getConfig();
			c->invert = cmd->val == 0 ? false : true;
			this->setConfig(*c);

		}else if(cmd->type == CMDtype::get){
			ButtonSourceConfig* c = this->getConfig();
			*reply+=std::to_string(c->invert);
		}else{
			*reply+="Err. invert: 1 else 0";
		}
	}else if(cmd->cmd == "spi_btncut"){
		if(cmd->type == CMDtype::set){
			ButtonSourceConfig* c = this->getConfig();
			c->cutRight = cmd->val == 0 ? false : true;
			this->setConfig(*c);

		}else if(cmd->type == CMDtype::get){
			ButtonSourceConfig* c = this->getConfig();
			*reply+=std::to_string(c->cutRight);

		}else{
			*reply+="Err. cut bytes right: 1 else 0";
		}
	}else{
		result = false;
	}
	return result;
}


ButtonSourceConfig SPI_Buttons::decodeIntToConf(uint16_t val){
	ButtonSourceConfig c;
	c.numButtons = val & 0x3F;
	c.invert = (val >> 6) & 0x1;
	c.cutRight = (val >> 7) & 0x1;
	c.extraOptions = (val >> 8);
	return c;
}
uint16_t SPI_Buttons::encodeConfToInt(ButtonSourceConfig* c){
	uint16_t val = c->numButtons & 0x3F;
	val |= c->invert << 6;
	val |= c->cutRight << 7;
	val |= c->extraOptions << 8;
	return val;
}

