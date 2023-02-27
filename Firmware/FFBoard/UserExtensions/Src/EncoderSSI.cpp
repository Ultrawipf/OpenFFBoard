/*
 * EncoderSSI.cpp
 *
 *  Created on: 22.02.23
 *      Author: Yannick
 *
 */

#include "EncoderSSI.h"

bool EncoderSSI::inUse = false;
ClassIdentifier EncoderSSI::info = {
		 .name = "SSI" ,
		 .id=CLSID_ENCODER_SSI
};

const ClassIdentifier EncoderSSI::getInfo(){
	return info;
}


EncoderSSI::EncoderSSI() :
		SPIDevice(ENCODER_SPI_PORT, ENCODER_SPI_PORT.getCsPins()[0]),
		CommandHandler("ssienc",CLSID_ENCODER_SSI,0) {
	EncoderSSI::inUse = true;


	restoreFlash();
	this->spiPort.takeExclusive(true);
	configSPI();
	registerCommands();
}

EncoderSSI::~EncoderSSI() {
	setPos(0);
	EncoderSSI::inUse = false;
	this->spiPort.takeExclusive(false);
}


void EncoderSSI::restoreFlash(){
	uint16_t buf;
	if(Flash_Read(ADR_SSI_CONF1, &buf)){
		this->lenghtDataBit = (buf & 0x1F)+1; // up to 32 bit. 5 bits
		this->spiSpeed = ((buf >> 5) & 0x3);
		setMode(static_cast<EncoderSSI_modes>((buf >> 8) & 0xF));
	}
	posOffset = Flash_ReadDefault(ADR_SSI_OFS, 0)<<std::max(0,(lenghtDataBit-16));

}

void EncoderSSI::saveFlash(){
	uint16_t buf = std::max((this->lenghtDataBit-1),0) & 0x1F;
	buf |= ((this->spiSpeed) & 0x3) << 5;
	// 1 bit free
	buf |= (((uint16_t)this->mode) & 0xf) << 8; // 4 bits
	Flash_Write(ADR_SSI_CONF1, buf);
	Flash_Write(ADR_SSI_OFS, posOffset >> std::max(0,(lenghtDataBit-16)));
}

void EncoderSSI::configSPI() {

	uint32_t prescale;
	switch (spiSpeed) {
		case 0 :
			prescale = SPI_BAUDRATEPRESCALER_32;
			break;
		case 1 :
			prescale = SPI_BAUDRATEPRESCALER_16;
			break;
		case 2 :
			prescale = SPI_BAUDRATEPRESCALER_8;
			break;
		default :
			prescale = SPI_BAUDRATEPRESCALER_32;
			break;
	}

	SPIConfig* config =  this->getSpiConfig();

	config->peripheral.BaudRatePrescaler = prescale;
	config->peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	config->peripheral.CLKPhase = SPI_PHASE_2EDGE;
	config->peripheral.CLKPolarity = SPI_POLARITY_LOW;
	config->peripheral.DataSize = SPI_DATASIZE_8BIT;
	config->cspol = true;
	this->setSpiConfig(*config);
	this->spiPort.configurePort(&config->peripheral);

}

bool EncoderSSI::isCreatable(){
	return !EncoderSSI::inUse && ENCODER_SPI_PORT.hasFreePins();
}

void EncoderSSI::spiRxCompleted(SPIPort* port) {
	lastPos = pos;
	newPos = 0;
	switch(mode){

	case EncoderSSI_modes::AMT23:
	{
		newPos = (uint16_t)(spi_buf[0] & 0x3F) << 8;
		newPos |= spi_buf[1];
		newPos >>= std::max(14-lenghtDataBit,0);
		uint8_t parity = (spi_buf[0] >> 6) & 0x3;
		for(uint8_t i = 0;i<7;i++){
			parity ^= (newPos >> (2*i)) & 0x3;
		}
		if(parity){
			newPos &= ((1<<lenghtDataBit) - 1);
			pos = newPos - (1<<(lenghtDataBit-1));
		}else{
			errors++;
		}

		break;
	}


	case EncoderSSI_modes::rawmsb:
	{
		for(uint8_t i = 0;i<bytes;i++){
			newPos |= spi_buf[i] << bytes;
		}
		newPos &= ((1<<lenghtDataBit) - 1);
		pos = newPos;
		break;
	}

	}
	// Handle overflow
	if(pos-lastPos > 1<<(lenghtDataBit-1)){
		mtpos--;
	}else if(lastPos-pos > 1<<(lenghtDataBit-1)){
		mtpos++;
	}
	waitData = false;
}

void EncoderSSI::beginSpiTransfer(SPIPort* port){
	//port->takeSemaphore();
	waitData = true;
	assertChipSelect();
}

void EncoderSSI::endSpiTransfer(SPIPort* port){
	clearChipSelect();
	//port->giveSemaphore();
}



EncoderType EncoderSSI::getEncoderType(){
	return EncoderType::absolute;
}


int32_t EncoderSSI::getPosAbs(){
	if(!waitData){ // If a transfer is still in progress return the last result
		spiPort.receive_DMA(spi_buf, transferlen, this); // Receive next frame

	}
	return pos + mtpos * getCpr();
}

int32_t EncoderSSI::getPos(){
	return getPosAbs()-posOffset;
}

void EncoderSSI::setPos(int32_t newpos){

	posOffset = pos - newpos;
}



uint32_t EncoderSSI::getCpr(){
	return 1<<lenghtDataBit;
}

void EncoderSSI::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("bits", EncoderSSI_commands::bits, "Bits of resolution",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("speed", EncoderSSI_commands::speed, "SPI speed preset 0-2",CMDFLAG_GET|CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("errors", EncoderSSI_commands::errors, "Error count",CMDFLAG_GET);
	registerCommand("mode", EncoderSSI_commands::mode, "SPI mode",CMDFLAG_INFOSTRING | CMDFLAG_GET | CMDFLAG_SET);
}

void EncoderSSI::setMode(EncoderSSI_modes mode){
	if(mode == EncoderSSI_modes::AMT23){
		transferlen = 2;
	}else{
		transferlen = ((lenghtDataBit-1) / 8)+1;
	}
	this->mode = mode;
}

std::string EncoderSSI::printSpeeds(){
	std::string reply;
	uint8_t base = 5; // 32
	for(uint8_t i = 0; i<3;i++){
		uint32_t khz = spiPort.getBaseClk() / ((1 << (base - i)) * 1000);
		reply+=  std::to_string(khz) + "kHz:" + std::to_string(i)+"\n";
	}
	return reply;
}

CommandStatus EncoderSSI::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<EncoderSSI_commands>(cmd.cmdId)){
	case EncoderSSI_commands::bits:
		handleGetSet(cmd, replies, this->lenghtDataBit);
		if(mode == EncoderSSI_modes::AMT23){
			lenghtDataBit = std::min(std::max(lenghtDataBit,12),14); // Only 12b and 14b valid
		}
		break;
	case EncoderSSI_commands::errors:
		replies.emplace_back(errors);
		break;
	case EncoderSSI_commands::speed:
		if(cmd.type == CMDtype::info){
			replies.emplace_back(printSpeeds());
		}else{
			handleGetSet(cmd, replies, this->spiSpeed);
			this->spiSpeed = clip(this->spiSpeed,0,2); // Limit from 0-2
			if(cmd.type == CMDtype::set){
				configSPI();
			}
		}
		break;
	case EncoderSSI_commands::mode:
		if(cmd.type == CMDtype::set){
			setMode(static_cast<EncoderSSI_modes>(cmd.val & 0xF));

		}else if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->mode);
		}else if(cmd.type == CMDtype::info){
			replies.emplace_back(printModes(this->mode_names));
		}else{
			return CommandStatus::ERR;
		}
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;

}
