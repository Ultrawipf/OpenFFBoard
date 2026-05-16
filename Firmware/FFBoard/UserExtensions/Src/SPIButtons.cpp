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

const std::vector<std::string> SPI_Buttons::mode_names = {"Thrustmaster/HEF4021BT","74xx165"};
const std::vector<std::string> SPI_Buttons::speed_names = {"Fast","Medium","Slow"};

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

SPI_Buttons_1::SPI_Buttons_1()
	: SPI_Buttons{ADR_SPI_BTN_1_CONF, ADR_SPI_BTN_1_CONF_2, &external_spi, 0} {
	restoreFlash(); // Call base class version (SPI_Buttons_1 doesn't override)
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

SPI_Buttons_2::SPI_Buttons_2()
	: SPI_Buttons{ADR_SPI_BTN_2_CONF, ADR_SPI_BTN_2_CONF_2, &external_spi, 1} {
	restoreFlash(); // Call base class version (SPI_Buttons_2 doesn't override)
}


// TODO check if pin is free
SPI_Buttons::SPI_Buttons(uint16_t configuration_address, uint16_t configuration_address_2, SPIPort* spiPort, uint8_t instance)
	: CommandHandler("spibtn",CLSID_BTN_SPI,instance), SPIDevice(*spiPort,spiPort->getFreeCsPins()[0]){

	this->configuration_address = configuration_address;
	this->configuration_address_2 = configuration_address_2;
	// NOTE: restoreFlash() is NOT called here - it must be called by derived class constructors
	// This is because calling virtual functions in base class constructor doesn't dispatch to derived class

	// Initialize with default settings - will be overwritten by restoreFlash() in derived class
	this->spiConfig.peripheral.BaudRatePrescaler = speedPresets[1]; // Medium speed default
	this->spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_LSB;

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
	registerCommand("spispeed", SPIButtons_commands::spispeed, "SPI speed preset",CMDFLAG_INFOSTRING | CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("debug", SPIButtons_commands::debug, "Debug raw SPI data",CMDFLAG_GET);
	registerCommand("syncread", SPIButtons_commands::syncread, "Test synchronous read",CMDFLAG_GET);
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
		spiPort.reserveCsPin(this->spiConfig.cs);
	}else{
		// CS pin not found - this is an error condition
		// Try to use first free pin as fallback
		auto& freePins = spiPort.getFreeCsPins();
		if(!freePins.empty()){
			this->spiConfig.cs = freePins[0];
			spiPort.reserveCsPin(this->spiConfig.cs);
		}
	}
	// Setup presets
	if(conf.mode == SPI_BtnMode::TM){
		this->spiConfig.cspol = true;
		this->conf.cutRight = true;
		this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_LOW;
		this->spiConfig.peripheral.CLKPhase = SPI_PHASE_1EDGE;

	}else if(conf.mode == SPI_BtnMode::PISOSR){
		this->spiConfig.cspol = false;
		this->conf.cutRight = false;
		this->spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
		this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH; // its actually shifting on the rising edge but 165 will have the first output set even before clocking. First clock cycle is actually second bit so we sample at the falling edge and skip the first bit with that.
	}
//	spiPort.takeSemaphore();
//	spiPort.configurePort(&this->spiConfig.peripheral);
//	spiPort.giveSemaphore();
	initSPI();
	if(config.numButtons == 64){ // Special case
			mask = 0xffffffffffffffff;
	}else{
		mask = (uint64_t)pow<uint64_t>(2,config.numButtons)-(uint64_t)1; // Must be done completely in 64 bit!
	}
	// Calculate offset: if numButtons is multiple of 8, offset is 0
	// Otherwise, offset is 8 - (numButtons % 8)
	offset = (config.numButtons % 8 == 0) ? 0 : (8 - (config.numButtons % 8));

	// Thrustmaster uses extra bits for IDs
	if(config.mode == SPI_BtnMode::TM){
		bytes = 1+((config.numButtons+2)/8);
	}else{
		bytes = 1+((config.numButtons-1)/8);
	}

	// Update ButtonSource::btnnum so getBtnNum() returns correct value
	this->btnnum = config.numButtons;
}

ButtonSourceConfig* SPI_Buttons::getConfig(){
	return &this->conf;
}

uint16_t SPI_Buttons::getBtnNum(){
	// Always return conf.numButtons as the source of truth
	// btnnum inheritance issue causes it to not update correctly
	return this->conf.numButtons;
}

void SPI_Buttons::setSpiSpeed(uint8_t speedPreset){
	speedPreset = clip<uint8_t,uint8_t>(speedPreset,0,this->speedPresets.size());
	this->conf.spi_speed = speedPreset;
	this->spiConfig.peripheral.BaudRatePrescaler = this->speedPresets[speedPreset];
	initSPI();
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
		return this->conf.numButtons;	// Return conf.numButtons instead of btnnum

	// CS pin and semaphore managed by spi port
	spiPort.receive_DMA(spi_buf, bytes, this);

	return this->conf.numButtons;	// Return conf.numButtons instead of btnnum
}

std::string SPI_Buttons::printModes(const std::vector<std::string>& names){
	std::string reply;
	for(uint8_t i = 0; i<names.size();i++){
		reply+=  names[i]  + ":" + std::to_string(i)+"\n";
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
			this->saveFlash(); // Save to flash immediately
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(this->getBtnNum());
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
			replies.emplace_back(this->getConfig()->invert ? 1 : 0);
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
			replies.emplace_back(this->getConfig()->cutRight ? 1 : 0);
		}else{
			return CommandStatus::ERR;
		}
		break;
	case SPIButtons_commands::mode:
		if(cmd.type == CMDtype::set){
			setMode((SPI_BtnMode)cmd.val);
			this->saveFlash(); // Save to flash immediately
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->conf.mode);
		}else if(cmd.type == CMDtype::info){
			replies.emplace_back(printModes(this->mode_names));
		}else{
			return CommandStatus::ERR;
		}
		break;

	case SPIButtons_commands::spispeed:
		if(cmd.type == CMDtype::set){
			setSpiSpeed(cmd.val);
			this->saveFlash(); // Save to flash immediately
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->conf.spi_speed);
		}else if(cmd.type == CMDtype::info){
			replies.emplace_back(printModes(this->speed_names));
		}else{
			return CommandStatus::ERR;
		}
		break;

	case SPIButtons_commands::cs:
		if (handleGetSet(cmd, replies, this->conf.cs_num) == CommandStatus::OK ) {
			setConfig(this->conf);
			this->saveFlash(); // Save to flash immediately
		}
		break;

	case SPIButtons_commands::debug:
		if(cmd.type == CMDtype::get){
			// Return raw SPI buffer data in hex format for debugging
			std::string debug_data = "Raw:";
			for(uint8_t i = 0; i < this->bytes; i++){
				char hex[4];
				sprintf(hex, "%02X", this->spi_buf[i]);
				debug_data += hex;
				if(i < this->bytes - 1) debug_data += " ";
			}
			debug_data += " offset:" + std::to_string(this->offset);
			debug_data += " mask:0x" + std::to_string(this->mask);
			debug_data += " inv:" + std::to_string(this->conf.invert);
			debug_data += " cut:" + std::to_string(this->conf.cutRight);
			replies.emplace_back(debug_data);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case SPIButtons_commands::syncread:
		if(cmd.type == CMDtype::get){
			// Detailed diagnostic of SPI communication
			std::string result = "";

			// Show which SPI port is being used
			SPI_HandleTypeDef* hspi = spiPort.getPortHandle();
			if(hspi->Instance == SPI2) result += "SPI2 ";
			else if(hspi->Instance == SPI3) result += "SPI3 ";
			else result += "SPI? ";

			// Show CS pin info
			result += "CS:" + std::to_string(this->conf.cs_num) + " ";

			// Force a synchronous read
			uint8_t test_buf[4] = {0xAA, 0xAA, 0xAA, 0xAA}; // Pre-fill with known pattern
			HAL_StatusTypeDef status = HAL_SPI_Receive(hspi, test_buf, this->bytes, 100);

			result += "HAL:" + std::to_string(status) + " ";
			result += "Data:";
			for(uint8_t i = 0; i < this->bytes; i++){
				char hex[4];
				sprintf(hex, "%02X", test_buf[i]);
				result += hex;
				if(i < this->bytes - 1) result += " ";
			}
			replies.emplace_back(result);
		}else{
			return CommandStatus::ERR;
		}
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
}

static ButtonSourceConfig decodeIntToConf(uint16_t config_int, uint16_t config_int_2){
	ButtonSourceConfig c;
	c.numButtons = (config_int & 0x3F) + 1;
	c.invert = (config_int >> 6) & 0x1;
	c.cutRight = (config_int >> 7) & 0x1;
	c.mode = SPI_BtnMode(config_int >> 8);
	c.cs_num = (config_int_2 & 0x3);
	c.spi_speed = (config_int_2 >> 3) & 0x3;
	return c;
}
static std::tuple<uint16_t, uint16_t> encodeConfToInt(ButtonSourceConfig* c){
	uint16_t val = (c->numButtons-1) & 0x3F; // 1-64
	val |= c->invert << 6;
	val |= c->cutRight << 7;
	val |= (uint16_t)c->mode << 8;
	uint16_t val2 = c->cs_num & 0x3;
	val2 |= (c->spi_speed & 0x3) << 3;

	return { val, val2 };
}

