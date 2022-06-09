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

MtEncoderSPI::MtEncoderSPI() : SPIDevice(ENCODER_SPI_PORT,ENCODER_SPI_PORT.getFreeCsPins()[0]), CommandHandler("mtenc",CLSID_ENCODER_MTSPI,0),cpp_freertos::Thread("MTENC",256,42) {
	MtEncoderSPI::inUse = true;
	this->spiConfig.peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; // 4 = 10MHz 8 = 5MHz
	this->spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	this->spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
	this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	this->spiConfig.cspol = true;

	restoreFlash();
	spiPort.reserveCsPin(this->spiConfig.cs);

	CommandHandler::registerCommands();
	registerCommand("cs", MtEncoderSPI_commands::cspin, "CS pin",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("pos", MtEncoderSPI_commands::pos, "Position",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("errors", MtEncoderSPI_commands::errors, "Parity error count",CMDFLAG_GET);
	this->Start();
}

MtEncoderSPI::~MtEncoderSPI() {
	MtEncoderSPI::inUse = false;
	spiPort.freeCsPin(this->spiConfig.cs);
}

void MtEncoderSPI::restoreFlash(){
	uint16_t conf_int = Flash_ReadDefault(ADR_MTENC_CONF1, 0);
	offset = Flash_ReadDefault(ADR_MTENC_OFS, 0) << 2;
	uint8_t cspin = conf_int & 0xF;
	setCsPin(cspin);
}

void MtEncoderSPI::saveFlash(){
	uint16_t conf_int = this->cspin & 0xF;
	Flash_Write(ADR_MTENC_CONF1, conf_int);
	Flash_Write(ADR_MTENC_OFS, offset >> 2);
}


void MtEncoderSPI::Run(){
	while(true){
		requestNewDataSem.Take(); // Wait until a position is requested
		//spiPort.receive_DMA(spi_buf, bytes, this); // Receive next frame
		updateAngleStatus();
		this->WaitForNotification();  // Wait until DMA is finished
		if(updateAngleStatusCb()){

			if(curAngleInt-lastAngleInt > 0x20000){ // Underflowed
				rotations--;
			}
			else if(lastAngleInt-curAngleInt > 0x20000){ // Overflowed
				rotations++;
			}
			lastAngleInt = curAngleInt;

			curPos = rotations * getCpr() + curAngleInt; // Update position
		}else{
			errors++;
		}
		waitForUpdateSem.Give();
		updateInProgress = false;
	}
}

void MtEncoderSPI::setCsPin(uint8_t cspin){
	spiPort.freeCsPin(this->spiConfig.cs);
	this->cspin = std::min<uint8_t>(spiPort.getCsPins().size(), cspin);
	this->spiConfig.cs = *spiPort.getCsPin(this->cspin);
	initSPI();
	spiPort.reserveCsPin(this->spiConfig.cs);
}

void MtEncoderSPI::initSPI(){
	spiPort.takeSemaphore();

	spiPort.configurePort(&this->spiConfig.peripheral);
	spiPort.giveSemaphore();
}

/**
 * MT encoder reads 1 byte and transmits 1 byte back after that
 */
uint8_t MtEncoderSPI::readSpi(uint8_t addr){

	uint8_t txbuf[2] = {(uint8_t)(addr | MAGNTEK_READ),0};
	uint8_t rxbuf[2] = {0,0};
	spiPort.transmitReceive(txbuf, rxbuf, 2, this,100);

	return rxbuf[1];
}

void MtEncoderSPI::writeSpi(uint8_t addr,uint8_t data){
	uint8_t txbuf[2] = {addr,data};
	spiPort.transmit(txbuf, 2, this,100);
}

void MtEncoderSPI::setPos(int32_t pos){
	offset = curPos - pos;
}


void MtEncoderSPI::spiTxRxCompleted(SPIPort* port){

	if(updateInProgress){
		NotifyFromISR();
		//updateAngleStatusCb();
		memcpy(rxbuf,rxbuf_t,4);
	}
}


/**
 * Reads the angle and diagnostic registers in burst mode
 */
void MtEncoderSPI::updateAngleStatus(){

	uint8_t txbufNew[4] = {0x03 | MAGNTEK_READ,0,0,0};
	memcpy(this->txbuf,txbufNew,4);

	spiPort.transmitReceive_DMA(txbuf, rxbuf_t, 4, this);


}

bool MtEncoderSPI::updateAngleStatusCb(){
	uint32_t angle17_10 = rxbuf[1];
	uint32_t angle9_4 = rxbuf[2];
	uint32_t angle3_0 = rxbuf[3];

	// Parity check byte 2
	uint8_t pc1 = angle17_10 ^ angle17_10 >> 1;
	pc1 = pc1 ^ pc1 >> 2;
	pc1 = pc1 ^ pc1 >> 4;

	uint8_t pc1_2 = angle9_4 ^ angle9_4 >> 1;
	pc1_2 = pc1_2 ^ pc1_2 >> 2;
	pc1_2 = pc1_2 ^ pc1_2 >> 4;

	// Parity check byte 1
	angle3_0 = angle3_0 >> 2; // shift 2
	uint8_t pc2 = (angle3_0) ^ (angle3_0) >> 1;
	pc2 = pc2 ^ pc2 >> 2;
	pc2 = pc2 ^ pc2 >> 4;

	nomag = 	(angle9_4 & 0x02) >> 1;
	overspeed = (angle3_0 & 0x04) >> 2;
	angle9_4 = 	(angle9_4 & 0xFC) >> 2;
	angle3_0 = 	angle3_0 >> 2;//(angle3_0 & 0xF0) >> 4;


	bool parity_ok = !(pc2 & 1) && ((pc1 & 1) == (pc1_2 & 1));

	curAngleInt = (angle17_10 << 10) | (angle9_4 << 4) | (angle3_0);


	this->updateInProgress = false;

	return parity_ok; // ok if both bytes have even parity
}

int32_t MtEncoderSPI::getPos(){

	return getPosAbs() - offset;
}

int32_t MtEncoderSPI::getPosAbs(){
	if(updateInProgress){ // If a transfer is still in progress return the last result
		return curPos;
	}
	updateInProgress = true;
	requestNewDataSem.Give(); // Start transfer
	waitForUpdateSem.Take(10); // Wait a bit

	return curPos;
}

uint32_t MtEncoderSPI::getCpr(){
	return 262144;
}



CommandStatus MtEncoderSPI::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<MtEncoderSPI_commands>(cmd.cmdId)){
	case MtEncoderSPI_commands::cspin:
		if(cmd.type==CMDtype::get){
			replies.emplace_back(this->cspin+1);
		}else if(cmd.type==CMDtype::set){
			this->setCsPin(cmd.val-1);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case MtEncoderSPI_commands::pos:
		if(cmd.type==CMDtype::get){
			replies.emplace_back(getPos());
		}else if(cmd.type==CMDtype::set){
			this->setPos(cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;
	case MtEncoderSPI_commands::errors:
		replies.emplace_back(errors);
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
}

#endif
