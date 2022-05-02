/*
 * ButtonSourceSPI.cpp
 *
 *  Created on: 11.02.2020
 *      Author: Yannick
 */

#include <math.h>
#include <tuple>

#include "constants.h"
#include <SPIButtons.h>
#include "eeprom_addresses.h"

static ButtonSourceConfig decodeIntToConf(uint16_t config, uint16_t config_int_2);
static std::tuple<uint16_t, uint16_t> encodeConfToInt(ButtonSourceConfig* c);

ClassIdentifier SPI_Buttons_1::info = {
		 .name = "SPI Buttons 1" ,
		 .id=CLSID_BTN_SPI,
 };
const ClassIdentifier SPI_Buttons_1::getInfo(){
	return info;
}

bool SPI_Buttons_1::isCreatable(){
	return (external_spi.hasFreePins());
}

ClassIdentifier SPI_Buttons_2::info = {
		 .name = "SPI Buttons 2" ,
		 .id=CLSID_BTN_SPI,
 };
const ClassIdentifier SPI_Buttons_2::getInfo(){
	return info;
}

bool SPI_Buttons_2::isCreatable(){
	return false;//(external_spi.hasFreePins();
}


// TODO check if pin is free
SPI_Buttons::SPI_Buttons(uint16_t configuration_address, uint16_t configuration_address_2)
	: CommandHandler("spibtn",CLSID_BTN_SPI,0), SPIDevice(external_spi,external_spi.getFreeCsPins()[0]){

	this->configuration_address = configuration_address;
	this->configuration_address_2 = configuration_address_2;
	this->spiConfig.peripheral.BaudRatePrescaler = SPIBUTTONS_SPEED;
	this->spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_LSB;
	this->spiConfig.peripheral.CLKPhase = SPI_PHASE_1EDGE;
	this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_LOW;

	restoreFlash();
	initSPI();

	registerCommands();
	this->setCommandsEnabled(true);
	ready  = true;
}



void SPI_Buttons::initSPI(){
	spiPort.takeSemaphore();
	spiPort.configurePort(&this->spiConfig.peripheral);
	spiPort.giveSemaphore();
}

SPI_Buttons::~SPI_Buttons() {

}


void SPI_Buttons::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("mode", SPIButtons_commands::mode, "SPI mode",CMDFLAG_INFOSTRING | CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("btncut", SPIButtons_commands::btncut, "Cut buttons right",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("btnpol", SPIButtons_commands::btnpol, "Invert",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("btnnum", SPIButtons_commands::btnnum, "Number of buttons",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("cs", SPIButtons_commands::cs, "SPI CS pin",CMDFLAG_GET | CMDFLAG_SET);
}

/**
 * Called on preset change
 */
void SPI_Buttons::setMode(SPI_BtnMode mode){
	this->conf.mode = mode;
	setConfig(conf);
}

void SPI_Buttons::setConfig(ButtonSourceConfig config){
	config.numButtons = std::min<uint8_t>(this->maxButtons, config.numButtons);
	this->conf = config;

	spiPort.freeCsPin(this->spiConfig.cs);
	OutputPin* newPin = spiPort.getCsPin(config.cs_num-1); // TODO update internal pin number if requested pin is blocked
	if(newPin != nullptr){
		this->spiConfig.cs = *newPin;

	}
	spiPort.reserveCsPin(this->spiConfig.cs);
	// Setup presets
	if(conf.mode == SPI_BtnMode::TM){
		this->spiConfig.cspol = true;
		this->conf.cutRight = true;
		this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_LOW;

	}else if(conf.mode == SPI_BtnMode::PISOSR){
		this->spiConfig.cspol = false;
		this->conf.cutRight = false;
		this->spiConfig.peripheral.CLKPhase = SPI_PHASE_1EDGE;
		this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_LOW;
	}
	spiPort.takeSemaphore();
	spiPort.configurePort(&this->spiConfig.peripheral);
	spiPort.giveSemaphore();
	if(config.numButtons == 64){ // Special case
			mask = 0xffffffffffffffff;
	}else{
		mask = (uint64_t)pow<uint64_t>(2,config.numButtons)-(uint64_t)1; // Must be done completely in 64 bit!
	}
	offset = 8 - (config.numButtons % 8);

	// Thrustmaster uses extra bits for IDs
	if(config.mode == SPI_BtnMode::TM){
		bytes = 1+((config.numButtons+2)/8);
	}else{
		bytes = 1+((config.numButtons-1)/8);
	}

	this->btnnum = config.numButtons;
}

ButtonSourceConfig* SPI_Buttons::getConfig(){
	return &this->conf;
}

void SPI_Buttons::saveFlash(){
	auto [configuration_int, cs_num_int] = encodeConfToInt(this->getConfig());

	Flash_Write(configuration_address, configuration_int);
	Flash_Write(configuration_address_2, cs_num_int);
}

void SPI_Buttons::restoreFlash(){
	uint16_t conf_int = Flash_ReadDefault(configuration_address, 0);
	uint16_t cs_num_int = Flash_ReadDefault(configuration_address_2, 1);

	setConfig(decodeIntToConf(conf_int, cs_num_int));
}

void SPI_Buttons::process(uint64_t* buf){
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
uint8_t SPI_Buttons::readButtons(uint64_t* buf){
	memcpy(buf,this->spi_buf,std::min<uint8_t>(this->bytes,8));
	process(buf); // give back last buffer

	if(spiPort.isTaken() || !ready)
		return this->btnnum;	// Don't wait.

	// CS pin and semaphore managed by spi port
	spiPort.receive_DMA(spi_buf, bytes, this);

	return this->btnnum;
}

std::string SPI_Buttons::printModes(){
	std::string reply;
	for(uint8_t i = 0; i<mode_names.size();i++){
		reply+=  mode_names[i]  + ":" + std::to_string(i)+"\n";
	}
	return reply;
}

CommandStatus SPI_Buttons::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<SPIButtons_commands>(cmd.cmdId)){
	case SPIButtons_commands::btnnum:
		if(cmd.type == CMDtype::set){
			ButtonSourceConfig* c = this->getConfig();
			c->numButtons = cmd.val;
			this->setConfig(*c);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->getBtnNum()));
		}else{
			return CommandStatus::ERR;
		}
		break;
	case SPIButtons_commands::btnpol:
		if(cmd.type == CMDtype::set){
			ButtonSourceConfig* c = this->getConfig();
			c->invert = cmd.val != 0;
			this->setConfig(*c);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->getConfig()->invert ? 1 : 0));
		}else{
			return CommandStatus::ERR;
		}
		break;
	case SPIButtons_commands::btncut:
		if(cmd.type == CMDtype::set){
			ButtonSourceConfig* c = this->getConfig();
			c->cutRight = cmd.val != 0;
			this->setConfig(*c);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->getConfig()->cutRight ? 1 : 0));
		}else{
			return CommandStatus::ERR;
		}
		break;
	case SPIButtons_commands::mode:
		if(cmd.type == CMDtype::set){
			setMode((SPI_BtnMode)cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply((uint8_t)this->conf.mode));
		}else if(cmd.type == CMDtype::info){
			replies.push_back(CommandReply(printModes()));
		}else{
			return CommandStatus::ERR;
		}
		break;

	case SPIButtons_commands::cs:
		if (handleGetSet(cmd, replies, this->conf.cs_num) == CommandStatus::OK ) {
			setConfig(this->conf);
		}
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
}

static ButtonSourceConfig decodeIntToConf(uint16_t config_int, uint16_t config_int_2){
	ButtonSourceConfig c;
	c.numButtons = config_int & 0x3F;
	c.invert = (config_int >> 6) & 0x1;
	c.cutRight = (config_int >> 7) & 0x1;
	c.mode = SPI_BtnMode(config_int >> 8);
	c.cs_num = static_cast<uint8_t>(config_int_2 & 0xF); // Leaving space for other use.
	return c;
}
static std::tuple<uint16_t, uint16_t> encodeConfToInt(ButtonSourceConfig* c){
	uint16_t val = c->numButtons & 0x3F;
	val |= c->invert << 6;
	val |= c->cutRight << 7;
	val |= (uint8_t)c->mode << 8;
	return { val, c->cs_num & 0xF };
}

