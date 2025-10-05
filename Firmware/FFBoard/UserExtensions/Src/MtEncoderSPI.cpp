/*
 * MtEncoderSPI.cpp
 *
 *  Created on: 02.11.2021
 *      Author: Yannick
 */

#include "MtEncoderSPI.h"
#include "constants.h"
#include "CRC.h"
#ifdef MTENCODERSPI
bool MtEncoderSPI::inUse = false;

ClassIdentifier MtEncoderSPI::info = {
		 .name = "MagnTek SPI" ,
		 .id=CLSID_ENCODER_MTSPI,
 };
const ClassIdentifier MtEncoderSPI::getInfo(){
	return info;
}

std::array<uint8_t,256> MtEncoderSPI::tableCRC __attribute__((section (".ccmram")));

MtEncoderSPI::MtEncoderSPI() : SPIDevice(ENCODER_SPI_PORT,ENCODER_SPI_PORT.getFreeCsPins()[0]), CommandHandler("mtenc",CLSID_ENCODER_MTSPI,0),cpp_freertos::Thread("MTENC",256,42) {
	MtEncoderSPI::inUse = true;
	this->spiConfig.peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; // 4 = 10MHz 8 = 5MHz
	this->spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	this->spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
	this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	this->spiConfig.cspol = true;

	//Init CRC-8 table
	makeCrcTable(tableCRC,POLY,8); // Mt6825, Poly X8+X2+X (+1)

	restoreFlash();
	spiPort.reserveCsPin(this->spiConfig.cs);

	CommandHandler::registerCommands();
	registerCommand("cs", MtEncoderSPI_commands::cspin, "CS pin",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("pos", MtEncoderSPI_commands::pos, "Position",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("errors", MtEncoderSPI_commands::errors, "Parity error count",CMDFLAG_GET);
	registerCommand("mode", MtEncoderSPI_commands::mode, "Encoder mode (MT6825=0;MT6835=1)",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
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
	MtEncoderSPI_mode mode = static_cast<MtEncoderSPI_mode>(conf_int >> 8);
	setMode(mode);
	setCsPin(cspin);
}

void MtEncoderSPI::saveFlash(){
	uint16_t conf_int = this->cspin & 0xF;
	conf_int |= ((uint8_t)mode & 0xf) << 8;
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
			int overflowLim = getCpr() >> 1;
			if(curAngleInt-lastAngleInt > overflowLim){ // Underflowed
				rotations--;
			}
			else if(lastAngleInt-curAngleInt > overflowLim){ // Overflowed
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
uint8_t MtEncoderSPI::readSpi(uint16_t addr){
	if(mode == MtEncoderSPI_mode::mt6825){
		uint8_t txbuf[2] = {(uint8_t)(addr | 0x80),0};
		uint8_t rxbuf[2] = {0,0};
		spiPort.transmitReceive(txbuf, rxbuf, 2, this,100);
	}else if(mode == MtEncoderSPI_mode::mt6835){
		uint8_t txbuf[3] = {(uint8_t)((addr & 0xf00) | 0x30),(uint8_t)(addr & 0xff),0};
		uint8_t rxbuf[3] = {0,0,0};
		spiPort.transmitReceive(txbuf, rxbuf, 3, this,100);
	}

	return rxbuf[1];
}

void MtEncoderSPI::writeSpi(uint16_t addr,uint8_t data){
	if(mode == MtEncoderSPI_mode::mt6825){
		uint8_t txbuf[2] = {(uint8_t)(addr & 0xff),data};
		spiPort.transmit(txbuf, 2, this,100);
	}else if(mode == MtEncoderSPI_mode::mt6835){
		uint8_t txbuf[3] = {(uint8_t)((addr & 0xf00) | 0x60),(uint8_t)(addr & 0xff),data};
		spiPort.transmit(txbuf, 3, this,100);
	}

}

void MtEncoderSPI::setPos(int32_t pos){
	offset = curPos - pos;
}


void MtEncoderSPI::spiTxRxCompleted(SPIPort* port){

	if(updateInProgress){
		NotifyFromISR();
		//updateAngleStatusCb();
		memcpy(rxbuf,rxbuf_t,sizeof(rxbuf));
	}
}


/**
 * Reads the angle and diagnostic registers in burst mode
 */
void MtEncoderSPI::updateAngleStatus(){



	if(mode == MtEncoderSPI_mode::mt6825){
		uint8_t txbufNew[5] = {0x03 | 0x80,0,0,0,0};
		memcpy(this->txbuf,txbufNew,5);
		spiPort.transmitReceive_DMA(txbuf, rxbuf_t, 4, this);
	}else if(mode == MtEncoderSPI_mode::mt6835){
		uint8_t txbufNew[6] = {0xA0,0x03,0,0,0,0};
		memcpy(this->txbuf,txbufNew,6);
		spiPort.transmitReceive_DMA(txbuf, rxbuf_t, 6, this);
	}


}

bool MtEncoderSPI::updateAngleStatusCb(){
	bool parity_ok = false;

	if(mode == MtEncoderSPI_mode::mt6825){
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


		parity_ok = !(pc2 & 1) && ((pc1 & 1) == (pc1_2 & 1));

		curAngleInt = (angle17_10 << 10) | (angle9_4 << 4) | (angle3_0);

	}else if(mode == MtEncoderSPI_mode::mt6835){
		uint32_t angle20_13 = rxbuf[2];
		uint32_t angle12_5 = rxbuf[3];
		uint32_t angle4_0 = (rxbuf[4] & 0xF8) >> 3;

		uint8_t status = rxbuf[4] & 0x7;
		nomag = 	(status & 0x02) >> 1;
		overspeed = (status & 0x01);
		uint8_t crc  = rxbuf[5];

		curAngleInt = (angle20_13 << 13) | (angle12_5 << 5) | (angle4_0);
		uint8_t calccrc = calculateCrc8(tableCRC, rxbuf+2, 3, 0);
		parity_ok = calccrc == crc;
	}


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
	switch(mode){
	case MtEncoderSPI_mode::mt6825:
		return 262144;
	case MtEncoderSPI_mode::mt6835:
		return 2097152;
	default:
		return 0; // Not possible
	}
}

void MtEncoderSPI::setMode(MtEncoderSPI::MtEncoderSPI_mode mode){
	this->mode = mode;
	// Reset variables
	this->curPos = 0;
	this->curAngleInt = 0;
	this->lastAngleInt = 0;
	this->rotations = 0;
	this->errors = 0;
	this->nomag = false;
	this->overspeed = false;
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
	case MtEncoderSPI_commands::mode:
		if(cmd.type==CMDtype::get){
			replies.emplace_back((uint8_t)mode);
		}else if(cmd.type==CMDtype::set){
			this->setMode((MtEncoderSPI_mode)cmd.val);
		}else if(cmd.type==CMDtype::info){
			replies.emplace_back("MT6825:0\nMT6835:1");
		}else{
			return CommandStatus::ERR;
		}
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
}

#endif
