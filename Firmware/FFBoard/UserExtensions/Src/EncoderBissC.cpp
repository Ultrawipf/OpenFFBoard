/*
 * EncoderBissC.cpp
 *
 *  Created on: 19.12.2020
 *      Author: Leon
 *      Update : Vincent
 *
 */

#include "EncoderBissC.h"

ClassIdentifier EncoderBissC::info = {
		 .name = "BissC" ,
		 .id=4,
		 .hidden = false,
};

const ClassIdentifier EncoderBissC::getInfo(){
	return info;
}

EncoderBissC::EncoderBissC() : SPIDevice(extra_stuff_spi, extra_stuff_spi.getFreeCsPins()[0]) {
	setPos(0);

	configSPI();

	//Init CRC table
	for(int i = 0; i < 64; i++){
		int crc = i;

		for (int j = 0; j < 6; j++){
			if (crc & 0x20){
				crc <<= 1;
				crc ^= POLY;
			} else {
				crc <<= 1;
			}
		}
		tableCRC6n[i] = crc;
	}
}

void EncoderBissC::configSPI() {

	uint32_t prescale = SPI_BAUDRATEPRESCALER_16;
	switch (spiSpeed) {
		case 1 :
			prescale = SPI_BAUDRATEPRESCALER_64;
			break;
		case 2 :
			prescale = SPI_BAUDRATEPRESCALER_32;
			break;
		default :
			prescale = SPI_BAUDRATEPRESCALER_16;
			break;
	}

	SPIConfig* config =  this->getSpiConfig();
	config->peripheral.BaudRatePrescaler = prescale;
	config->peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	config->peripheral.CLKPhase = SPI_PHASE_2EDGE;
	config->peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	config->peripheral.DataSize = SPI_DATASIZE_8BIT;
	this->setSpiConfig(*config);
}


EncoderBissC::~EncoderBissC() {
	setPos(0);
}

void EncoderBissC::acquirePosition(){
	// CS pin and semaphore managed by spi port
	waitData = true;
	spiPort.receive_DMA(spi_buf, bytes, this);
}

void EncoderBissC::spiRxCompleted(SPIPort* port) {
	memcpy(this->decod_buf,this->spi_buf,this->bytes);
	waitData = false;
}



EncoderType EncoderBissC::getType(){
	return EncoderType::absolute;
}

int32_t EncoderBissC::getPos(){
	acquirePosition();
	while (waitData) {
		__NOP();
	}
	int32_t lastpos = pos;

	//Put data into 64bit int to enable easy shifting
	uint64_t rxData64;
	rxData64 = (uint64_t)decod_buf[0] << 56;
	rxData64 |= (uint64_t)decod_buf[1] << 48;
	rxData64 |= (uint64_t)decod_buf[2] << 40;
	rxData64 |= (uint64_t)decod_buf[3] << 32;
	rxData64 |= (uint64_t)decod_buf[4] << 24;
	rxData64 |= (uint64_t)decod_buf[5] << 16;
	rxData64 |= (uint64_t)decod_buf[6] << 8;
	rxData64 |= (uint64_t)decod_buf[7];

	// sample of rxData64
	// like this 1100000000000000100001100111010000000101110111100000000000000000
	rxData64 <<= __builtin_clzll(rxData64);		// slice rxData to have a value starting with 1
	rxData64 &= 0x3FFFFFFFFFFFFFFF; 			// remove the 2 first bit

	// remove the first 1, count how many digit stay in buffer after removing the 0, if there is more than 32 digits,
	// keep only 32st (on the left)
	// 32 because the format is : (1+1+lenghtDataBit+1+1+6) - Align bitstream to left (Startbit, CDS, 22-bit Position, Error, Warning, CRC)
	int nbBit = log2(rxData64)+1;
	if ( nbBit >= ( lenghtDataBit + 10 ) ) {
		rxData64 >>= nbBit-( lenghtDataBit + 10 );
	}

	uint8_t crcRx = rxData64 & 0x3F; 									 //extract last 6-bit digits to get CRC
	uint32_t dataRx = (rxData64 >> 6) & ((1<<(lenghtDataBit + 2)) - 1);  //Shift out CRC, AND with 24-bit mask to get raw data (position, error, warning)
	pos = (dataRx >> 2) & ((1<<lenghtDataBit) - 1); 									 			 //Shift out error and warning, AND with 22-bit mask to get position


	uint8_t crc = 0;  //CRC seed is 0b000000
	crc = ((dataRx >> 30) & 0x03);
	crc = tableCRC6n[((dataRx >> 24) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 18) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 12) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 6) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 0) & 0x3F) ^ crc];
	crc = 0x3F & ~crc; //CRC is output inverted

	crc_ok = crc == crcRx;
	if(!crc_ok || pos < 0)
	{
		numErrors++;
	}

	//handle multiturn
	if(pos-lastpos > 1<<(lenghtDataBit-1)){
		mtpos--;
	}else if(lastpos-pos > 1<<(lenghtDataBit-1)){
		mtpos++;
	}

	return pos + mtpos * getCpr() - posOffset;
}

void EncoderBissC::setPos(int32_t newpos){

	posOffset = pos - newpos;
}

uint32_t EncoderBissC::getCpr(){
	return 1<<lenghtDataBit;
}

ParseStatus EncoderBissC::command(ParsedCommand* cmd,std::string* reply){
	// Prefix set but not our prefix
	if (cmd->prefix != this->getInfo().unique && cmd->prefix != '\0') {
		return ParseStatus::NOT_FOUND;
	}
	ParseStatus status = ParseStatus::OK;
	if (cmd->cmd == "bissCnbBitData") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string(lenghtDataBit);
		} else if (cmd->type == CMDtype::set) {
			lenghtDataBit = cmd->val;
		}
	} else if (cmd->cmd == "bissCsetSpeed") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string(spiSpeed);
		} else if (cmd->type == CMDtype::set) {
			spiSpeed = cmd->val;
			if ( spiSpeed < 1 || 3 < spiSpeed ) spiSpeed = 3;
			configSPI();
		}
	} else if (cmd->cmd == "bissCnbError") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string(numErrors);
		}
	} else if (cmd->cmd == "bissCgetRawPos") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string(pos) + " crc" + std::to_string(crc_ok);
		}
	} else {
		status = ParseStatus::NOT_FOUND;
	}
	return status;
}
