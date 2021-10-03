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

	SPIConfig* config =  this->getSpiConfig();
	config->peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
	config->peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	config->peripheral.CLKPhase = SPI_PHASE_1EDGE;
	config->peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	config->peripheral.DataSize = SPI_DATASIZE_8BIT;
	this->setSpiConfig(*config);

	//attachToPort(external_spi);

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
	//volatile int test = clz64(0b0000010000000000000000000000000000000000000000000000000000000000);
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

	decod_buf[0] &= 0x3F; //remove first 2 bits (startbit, maybe one ack)

	//Put data into 64bit int to enable easy shifting
	uint64_t rxData64 = (uint64_t)decod_buf[0] << 56;
	rxData64 |= (uint64_t)decod_buf[1] << 48;
	rxData64 |= (uint64_t)decod_buf[2] << 40;
	rxData64 |= (uint64_t)decod_buf[3] << 32;
	rxData64 |= (uint64_t)decod_buf[4] << 24;
	rxData64 |= (uint64_t)decod_buf[5] << 16;
	rxData64 |= (uint64_t)decod_buf[6] << 8;
	rxData64 |= (uint64_t)decod_buf[7];

	// sample of rxData64
	// slice rxData to have a value starting with 1
	// like this 1000000000000000000000000000110111111001111111100011111110000000
	rxData64 <<= __builtin_clzll(rxData64);

	// remove the first 1, count how many digit stay in buffer after removing the 0, if there is more than 32 digits,
	// keep only 32st (on the left)
	// 32 because the format is : (1+1+22+1+1+6) - Align bitstream to left (Startbit, CDS, 22-bit Position, Error, Warning, CRC)
	rxData64 <<= 1; // remove first
	int nbBit = log2(rxData64)+1;
	if (nbBit >= 32) {
		rxData64 >>= nbBit-32;
	}

	uint8_t crcRx = rxData64 & 0x3F; 				//extract last 6-bit digits to get CRC
	uint32_t dataRx = (rxData64 >> 6) & 0xFFFFFF;  //Shift out CRC, AND with 24-bit mask to get raw data (position, error, warning)
	pos = (dataRx >> 2) & 0x3FFFFF; 						//Shift out error and warning, AND with 22-bit mask to get position

	uint8_t crc = 0;  //CRC seed is 0b000000

	crc = tableCRC6n[((dataRx >> 18) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 12) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 6) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 0) & 0x3F) ^ crc];

	crc = 0x3F & ~crc; //CRC is output inverted

	if(crc != crcRx || pos < 0)
	{
		numErrors++;
		//TODO pos = -1;	//position is 22bit value, so in positive range. if crc error, negative value is indicator
	}

	//handle multiturn
	if(pos-lastpos > 1<<21){
		mtpos--;
	}else if(lastpos-pos > 1<<21){
		mtpos++;
	}

	return pos + mtpos * getCpr();
}

void EncoderBissC::setPos(int32_t pos){
	this->pos = pos; //TODO change to create offset
}

uint32_t EncoderBissC::getCpr(){
	return 1<<22; //TODO flexible
}

ParseStatus EncoderBissC::command(ParsedCommand* cmd,std::string* reply){
	return ParseStatus::NOT_FOUND;
}
