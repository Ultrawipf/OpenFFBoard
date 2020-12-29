/*
 * ButtonSourceSPI.cpp
 *
 *  Created on: 11.02.2020
 *      Author: Yannick
 */

#include <math.h>

#include "constants.h"
#include <SPIButtons.h>
#include "eeprom_addresses.h"

ClassIdentifier SPI_Buttons_1::info = {
		 .name = "SPI 1" ,
		 .id=1
 };
const ClassIdentifier SPI_Buttons_1::getInfo(){
	return info;
}

ClassIdentifier SPI_Buttons_2::info = {
		 .name = "SPI 2" ,
		 .id=2
 };
const ClassIdentifier SPI_Buttons_2::getInfo(){
	return info;
}

SPI_Buttons::SPI_Buttons(OutputPin &cs, uint16_t configuration_address)
	: spi_config{cs}, configuration_address{configuration_address} {

	this->spi_config.peripheral.BaudRatePrescaler = SPIBUTTONS_SPEED;
	this->spi_config.peripheral.FirstBit = SPI_FIRSTBIT_LSB;
	this->spi_config.peripheral.CLKPhase = SPI_PHASE_1EDGE;
	this->spi_config.peripheral.CLKPolarity = SPI_POLARITY_LOW;

	restoreFlash();
	initSPI();

	this->setCommandsEnabled(true);
}

const SPIConfig& SPI_Buttons::getConfig() const {
	return spi_config;
}

void SPI_Buttons::beginRequest(SPIPort::Pipe& pipe) {
	pipe.beginRx(spi_buf, MIN(bytes, 4));
}

void SPI_Buttons::initSPI(){
	attachToPort(external_spi);
}

SPI_Buttons::~SPI_Buttons() {

}

/*
 * Called on preset change
 */
void SPI_Buttons::setMode(SPI_BtnMode mode){
	this->conf.mode = mode;
	setConfig(conf);
}

void SPI_Buttons::setConfig(ButtonSourceConfig config){
	config.numButtons = MIN(this->maxButtons, config.numButtons);
	this->conf = config;

	// Setup presets
	if(conf.mode == SPI_BtnMode::TM){
		this->conf.cspol = false;
		this->conf.cutRight = true;
		this->spi_config.peripheral.CLKPolarity = SPI_POLARITY_LOW;

	}else if(conf.mode == SPI_BtnMode::PISOSR){
		this->conf.cspol = true;
		this->spi_config.peripheral.CLKPhase = SPI_PHASE_1EDGE;
		this->spi_config.peripheral.CLKPolarity = SPI_POLARITY_LOW;
	}

	mask = pow(2,config.numButtons)-1;
	offset = 8 - (config.numButtons % 8);
	bytes = 1+((config.numButtons-1)/8);
	this->btnnum = config.numButtons;
}

ButtonSourceConfig* SPI_Buttons::getConfig(){
	return &this->conf;
}

void SPI_Buttons::saveFlash(){
	Flash_Write(configuration_address, SPI_Buttons::encodeConfToInt(this->getConfig()));
}

void SPI_Buttons::restoreFlash(){
	uint16_t confint = 0;
	Flash_Read(configuration_address, &confint);
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

	if(this->requestPending())
		return;	// Don't wait.

	// Get next buffer
	requestPort();
}

void SPI_Buttons::printModes(std::string* reply){
	for(uint8_t i = 0; i<mode_names.size();i++){
		*reply+=  mode_names[i]  + ":" + std::to_string(i)+"\n";
	}
}

ParseStatus SPI_Buttons::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus result = ParseStatus::OK;
	auto prefix{"spi" + std::to_string(static_cast<CommandHandler&>(*this).getInfo().id) + "_"};

	if(cmd->cmd == prefix + "btnnum"){
		if(cmd->type == CMDtype::set){
			ButtonSourceConfig* c = this->getConfig();
			c->numButtons = cmd->val;
			this->setConfig(*c);

		}else if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->getBtnNum());
		}else{
			*reply+="Err. Supply number of buttons";
		}
	}else if(cmd->cmd == prefix + "btnpol"){
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
	}else if(cmd->cmd == prefix + "btncut"){
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
	}else if(cmd->cmd == prefix + "btn_mode"){
		if(cmd->type == CMDtype::set){
			setMode((SPI_BtnMode)cmd->val);
		}else if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint8_t)this->conf.mode);
		}else{
			printModes(reply);
		}
	}else if(cmd->cmd == "help"){
		result = ParseStatus::OK_CONTINUE;
		*reply += "SPI Button: spi#_btn_mode,spi#_btncut,spi#_btnpol,spi#_btnnum\n";
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

