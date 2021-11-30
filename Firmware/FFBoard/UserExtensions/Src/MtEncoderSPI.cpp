/*
 * MtEncoderSPI.cpp
 *
 *  Created on: 02.11.2021
 *      Author: Yannick
 */

#include "MtEncoderSPI.h"
#include "constants.h"
#ifdef MTENCODERSPI
bool MtEncoderSPI::inUse = false;

ClassIdentifier MtEncoderSPI::info = {
		 .name = "MT6825 SPI3" ,
		 .id=CLSID_ENCODER_MTSPI,
 };
const ClassIdentifier MtEncoderSPI::getInfo(){
	return info;
}

MtEncoderSPI::MtEncoderSPI() : SPIDevice(ext3_spi,ext3_spi.getFreeCsPins()[0]), CommandHandler("mtenc",CLSID_ENCODER_MTSPI,0) {
	MtEncoderSPI::inUse = true;
	this->spiConfig.peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; // 10MHz
	this->spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	this->spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
	this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	this->spiConfig.cspol = true;
	restoreFlash();

	CommandHandler::registerCommands();
	registerCommand("cs", MtEncoderSPI_commands::cspin, "CS pin");
}

MtEncoderSPI::~MtEncoderSPI() {
	MtEncoderSPI::inUse = false;
	spiPort.freeCsPin(this->spiConfig.cs);
}

void MtEncoderSPI::restoreFlash(){
	uint16_t conf_int = Flash_Read(ADR_MTENC_CONF1, 0);
	uint8_t cspin = conf_int & 0xF;
	setCsPin(cspin);
}

void MtEncoderSPI::saveFlash(){
	uint16_t conf_int = this->cspin & 0xF;
	Flash_Write(ADR_MTENC_CONF1, conf_int);
}

void MtEncoderSPI::setCsPin(uint8_t cspin){
	this->cspin = std::min<uint8_t>(spiPort.getCsPins().size(), cspin);
	this->spiConfig.cs = *spiPort.getCsPin(this->cspin);
	initSPI();
}

void MtEncoderSPI::initSPI(){
	spiPort.takeSemaphore();
	spiPort.freeCsPin(this->spiConfig.cs);

	spiPort.configurePort(&this->spiConfig.peripheral);
	spiPort.giveSemaphore();
}

/**
 * MT encoder reads 1 byte and transmits 1 byte back after that
 */
uint8_t MtEncoderSPI::readSpi(uint8_t addr){

	uint8_t txbuf[2] = {(uint8_t)(addr | MAGNTEK_READ),0};
	uint8_t rxbuf[2] = {0,0};
	spiPort.transmitReceive(txbuf, rxbuf, 2, this, 1000);
	return rxbuf[1];
}

void MtEncoderSPI::writeSpi(uint8_t addr,uint8_t data){
	uint8_t txbuf[2] = {addr,data};
	spiPort.transmit(txbuf, 2, this, 1000);
}

void MtEncoderSPI::setPos(int32_t pos){
	offset = curPos - pos;
}

/**
 * Reads the angle and diagnostic registers in burst mode
 */
void MtEncoderSPI::updateAngleStatus(){
	uint8_t txbuf[4] = {0x03 | MAGNTEK_READ,0,0,0};
	uint8_t rxbuf[4] = {0,0,0,0};
	spiPort.transmitReceive(txbuf, rxbuf, 4, this, 1000);

	uint32_t angle17_10 = rxbuf[1];
	uint32_t angle9_4 = rxbuf[2];
	uint32_t angle3_0 = rxbuf[3];

	nomag = 	(angle9_4 & 0x02) >> 1;
	overspeed = (angle3_0 & 0x04) >> 2;
	angle9_4 = 	(angle9_4 & 0xFC) >> 2;
	angle3_0 = 	(angle3_0 & 0xE0) >> 5;

	curAngleInt = (angle3_0) | (angle9_4 << 4) | (angle17_10 << 10);

	if(lastAngleInt < 0x10000 && curAngleInt > 0x30000){ // Underflowed
		rotations--;
	}
	else if(curAngleInt < 0x10000 && lastAngleInt > 0x30000){ // Overflowed
		rotations++;
	}

	curPos = rotations * getCpr() + curAngleInt;
	lastAngleInt = curAngleInt;
}


int32_t MtEncoderSPI::getPos(){
	updateAngleStatus();
	return curPos - offset;
}

int32_t MtEncoderSPI::getPosAbs(){
	updateAngleStatus();
	return curPos;
}

uint32_t MtEncoderSPI::getCpr(){
	return 262144;
}



CommandStatus MtEncoderSPI::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<MtEncoderSPI_commands>(cmd.cmdId)){
	case MtEncoderSPI_commands::cspin:
		if(cmd.type==CMDtype::get){
			replies.push_back(CommandReply(this->cspin+1));
		}else if(cmd.type==CMDtype::set){
			this->setCsPin(cmd.val);
		}else{
			return CommandStatus::ERR;
		}
	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
}

#endif
