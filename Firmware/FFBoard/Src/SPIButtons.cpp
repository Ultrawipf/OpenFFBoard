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

	initSPI();

	this->setCommandsEnabled(true);
}

void SPI_Buttons::initSPI(){
	spi = &HSPI2;
	cspin = SPI2_NSS_Pin;
	csport = SPI2_NSS_GPIO_Port;
	this->spi->Init.BaudRatePrescaler = SPIBUTTONS_SPEED;
	this->spi->Init.FirstBit = SPI_FIRSTBIT_LSB;
	this->spi->Init.CLKPhase = SPI_PHASE_1EDGE;
	this->spi->Init.CLKPolarity = SPI_POLARITY_LOW;



	// Setup presets
	if(conf.mode == SPI_BtnMode::TM){
		this->conf.cspol = false;

		this->spi->Init.CLKPolarity = SPI_POLARITY_LOW;

	}else if(conf.mode == SPI_BtnMode::PISOSR){
		this->conf.cspol = true;
		this->spi->Init.CLKPolarity = SPI_POLARITY_HIGH;
	}

	HAL_SPI_Init(this->spi);
}

SPI_Buttons::~SPI_Buttons() {

}

/*
 * Called on preset change
 */
void SPI_Buttons::setMode(SPI_BtnMode mode){
	this->conf.mode = mode;

	if(conf.mode == SPI_BtnMode::TM){
		this->conf.cspol = false;
		this->conf.cutRight = true;


	}else if(conf.mode == SPI_BtnMode::PISOSR){
		this->conf.cspol = true;

	}

	setConfig(conf);
	initSPI(); // Reinit spi
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

__attribute__((optimize("-Ofast")))
void SPI_Buttons::readButtons(uint32_t* buf){

	memcpy(buf,this->spi_buf,MIN(this->bytes,4));
	process(buf); // give back last buffer

	if(this->spibusy)
		return;	// Don't wait.

	// Get next buffer
	HAL_GPIO_WritePin(this->csport,this->cspin,conf.cspol ? GPIO_PIN_SET : GPIO_PIN_RESET);
	spibusy = true;
	HAL_SPI_Receive_DMA(this->spi,spi_buf, MIN(this->bytes,4));
}

void SPI_Buttons::SpiRxCplt(SPI_HandleTypeDef *hspi){
	if(hspi == this->spi && spibusy){
		spibusy = false;
		HAL_GPIO_WritePin(this->csport,this->cspin,conf.cspol ? GPIO_PIN_RESET : GPIO_PIN_SET);
	}
}

void SPI_Buttons::printModes(std::string* reply){
	for(uint8_t i = 0; i<mode_names.size();i++){
		*reply+=  mode_names[i]  + ":" + std::to_string(i)+"\n";
	}
}

ParseStatus SPI_Buttons::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus result = ParseStatus::OK;
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
			result = ParseStatus::ERR;
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
	}else if(cmd->cmd == "spibtn_mode"){
		if(cmd->type == CMDtype::set){
			setMode((SPI_BtnMode)cmd->val);
		}else if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint8_t)this->conf.mode);
		}else{
			printModes(reply);
		}
	}else if(cmd->cmd == "help"){
		result = ParseStatus::OK_CONTINUE;
		*reply += "SPI Button: spibtn_mode,spi_btncut,spi_btnpol,spi_btnnum\n";
	}else{
		result = ParseStatus::NOT_FOUND;
	}
	return result;
}


ButtonSourceConfig SPI_Buttons::decodeIntToConf(uint16_t val){
	ButtonSourceConfig c;
	c.numButtons = val & 0x3F;
	c.invert = (val >> 6) & 0x1;
	c.cutRight = (val >> 7) & 0x1;
	c.mode = SPI_BtnMode(val >> 8);
	return c;
}
uint16_t SPI_Buttons::encodeConfToInt(ButtonSourceConfig* c){
	uint16_t val = c->numButtons & 0x3F;
	val |= c->invert << 6;
	val |= c->cutRight << 7;
	val |= (uint8_t)c->mode << 8;
	return val;
}

