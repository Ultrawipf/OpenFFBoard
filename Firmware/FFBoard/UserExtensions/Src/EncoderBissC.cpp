/*
 * EncoderBissC.cpp
 *
 *  Created on: 19.12.2020
 *      Author: Leon
 *      Update : Yannick & Vincent
 *
 */

#include "EncoderBissC.h"
#include "CRC.h"
#include "array"

bool EncoderBissC::inUse = false;
ClassIdentifier EncoderBissC::info = {
		 .name = "BISS-C" ,
		 .id=CLSID_ENCODER_BISS
};

const ClassIdentifier EncoderBissC::getInfo(){
	return info;
}

std::array<uint8_t,64> EncoderBissC::tableCRC6n __attribute__((section (".ccmram")));

EncoderBissC::EncoderBissC() :
		SPIDevice(ENCODER_SPI_PORT, ENCODER_SPI_PORT.getCsPins()[0]),
		CommandHandler("bissenc",CLSID_ENCODER_BISS,0),
		cpp_freertos::Thread("BISSENC",64,42) {
	EncoderBissC::inUse = true;


	//Init CRC-6 table
	makeCrcTable(tableCRC6n,POLY,6);

	restoreFlash();
	this->spiPort.takeExclusive(true);
	configSPI();
	registerCommands();
	this->Start();
}

void EncoderBissC::Run(){
	bool first = true;
	while(true){
		requestNewDataSem.Take(); // Wait until a position is requested
		waitData = true;
		spiPort.receive_DMA(spi_buf, bytes, this); // Receive next frame
		this->WaitForNotification();  // Wait until DMA is finished

		if(updateFrame()){
			int32_t halfres = 1<<(lenghtDataBit-1);
			pos = newPos;
			if(first){ // Prevent immediate multiturn update
				lastPos = posOffset;
				first = false;
				// If offset from current pos is more than half rotation add a multiturn count by setting previous position to the reloaded offset
			}
			//handle multiturn
			if(pos-lastPos > halfres){
				mtpos--;
			}else if(lastPos-pos > halfres){
				mtpos++;
			}

			lastPos = pos;
		}else{
			numErrors++;
		}
		waitData = false;
		lastUpdateTick = HAL_GetTick();
		if(useWaitSem)
			waitForUpdateSem.Give();

	}
}


void EncoderBissC::restoreFlash(){
	uint16_t buf;
	if(Flash_Read(ADR_BISSENC_CONF1, &buf)){
		this->lenghtDataBit = (buf & 0x1F)+1; // up to 32 bit. 5 bits
		this->spiSpeed = ((buf >> 5) & 0x3) +1;
		this->invertDirection =  ((buf >> 7) & 1);
	}
	uint16_t restoredOffset = ( (uint16_t)(Flash_ReadDefault(ADR_BISSENC_OFS, 0) ));
	posOffset = static_cast<int32_t>(restoredOffset) << std::max(0,(lenghtDataBit-16));
}

void EncoderBissC::saveFlash(){
	uint16_t buf = std::max((this->lenghtDataBit-1),0) & 0x1F;
	buf |= ((this->spiSpeed-1) & 0x3) << 5;
	buf |= (this->invertDirection & 1) << 7;
	Flash_Write(ADR_BISSENC_CONF1, buf);
	int32_t scaledOfs = posOffset >> std::max(0,(lenghtDataBit-16));
	Flash_Write(ADR_BISSENC_OFS, (uint16_t)(scaledOfs) );
}

void EncoderBissC::configSPI() {

	uint32_t prescale;
	switch (spiSpeed) {
		case 1 :
			prescale = SPI_BAUDRATEPRESCALER_64;
			break;
		case 2 :
			prescale = SPI_BAUDRATEPRESCALER_32;
			break;
		case 3 :
			prescale = SPI_BAUDRATEPRESCALER_16;
			break;
		default :
			prescale = SPI_BAUDRATEPRESCALER_16;
			break;
	}

	SPIConfig* config =  this->getSpiConfig();
	// CS pin not used because bus must be always active and can't be shared!
	config->peripheral.BaudRatePrescaler = prescale;
	config->peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	config->peripheral.CLKPhase = SPI_PHASE_2EDGE;
	config->peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	config->peripheral.DataSize = SPI_DATASIZE_8BIT;
	config->cspol = true;
	this->setSpiConfig(*config);
	this->spiPort.configurePort(&config->peripheral);

}

bool EncoderBissC::isCreatable(){
	return !EncoderBissC::inUse && ENCODER_SPI_PORT.hasFreePins();
}

EncoderBissC::~EncoderBissC() {
	setPos(0);
	EncoderBissC::inUse = false;
	this->spiPort.takeExclusive(false);
}

void EncoderBissC::spiRxCompleted(SPIPort* port) {
	memcpy(this->decod_buf,this->spi_buf,this->bytes);
	this->NotifyFromISR();

}

void EncoderBissC::beginSpiTransfer(SPIPort* port){
	//port->takeSemaphore();
	assertChipSelect();
}

void EncoderBissC::endSpiTransfer(SPIPort* port){
	//port->giveSemaphore();
	clearChipSelect();
}



EncoderType EncoderBissC::getEncoderType(){
	return EncoderType::absolute;
}

__attribute__((optimize("-Ofast")))
bool EncoderBissC::updateFrame(){


	//Put data into 64bit int to enable easy shifting
	uint64_t rxData64;

	// 32b operation reduces time needed
	rxData64 = (uint64_t)__REV(decod_buf[0]) << 32;
	rxData64 |= (uint64_t)__REV(decod_buf[1]);

	// sample of rxData64
	// like this 1100000000000000100001100111010000000101110111100000000000000000
	rxData64 <<= __builtin_clzll(rxData64);		// slice rxData to have a value starting with 1
	rxData64 &= 0x3FFFFFFFFFFFFFFF; 			// remove the 2 first bit

	// remove the first 1, count how many digit stay in buffer after removing the 0, if there is more than 32 digits,
	// keep only 32st (on the left)
	// 32 because the format is : (1+1+lenghtDataBit+1+1+6) - Align bitstream to left (Startbit, CDS, 22-bit Position, Error, Warning, CRC)
	//int nbBit = log2(rxData64)+1;
	int nbBit =  64-__builtin_clzll(rxData64); // Much faster than log2
	if ( nbBit >= ( lenghtDataBit + 10 ) ) {
		rxData64 >>= nbBit-( lenghtDataBit + 10 );
	}

	uint8_t crcRx = rxData64 & 0x3F; 									 //extract last 6-bit digits to get CRC
	uint32_t dataRx = (rxData64 >> 6) & ((1<<(lenghtDataBit + 2)) - 1);  //Shift out CRC, AND with 24-bit mask to get raw data (position, error, warning)
	uint8_t errorWarning = (dataRx & 0x3); // Error and warning are lowest 2 bits now
	newPos = (dataRx >> 2) & ((1<<lenghtDataBit) - 1); 			//Shift out error and warning, AND with 22-bit mask to get position


	uint8_t crc = 0;  //CRC seed is 0b000000
	crc = ((dataRx >> 30) & 0x03);
	crc = tableCRC6n[((dataRx >> 24) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 18) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 12) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 6) & 0x3F) ^ crc];
	crc = tableCRC6n[((dataRx >> 0) & 0x3F) ^ crc];
	crc = 0x3F & ~crc; //CRC is output inverted

	bool crc_ok = crc == crcRx;
	return crc_ok;
}

int32_t EncoderBissC::getPosAbs(){
	if(!waitData){ // If a transfer is still in progress return the last result
		requestNewDataSem.Give(); // Start transfer
		if(useWaitSem && HAL_GetTick() - lastUpdateTick > waitThresh)
			waitForUpdateSem.Take(waitThresh); // Wait a bit
	}
	int32_t curpos = pos + mtpos * getCpr();
	return invertDirection ? -curpos : curpos;
}

int32_t EncoderBissC::getPos(){
	if(invertDirection){
		return getPosAbs()+posOffset;
	}else{
		return getPosAbs()-posOffset;
	}

}

void EncoderBissC::setPos(int32_t newpos){
	if(invertDirection){
		newpos = -newpos;
	}
	int32_t diff = ( pos - newpos);
	mtpos = newpos / getCpr(); // Multiturn should be reset

	posOffset = diff % getCpr(); // Always positive
}



uint32_t EncoderBissC::getCpr(){
	return 1<<lenghtDataBit;
}

void EncoderBissC::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("bits", EncoderBissC_commands::bits, "Bits of resolution",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("speed", EncoderBissC_commands::speed, "SPI speed preset 1-3",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("errors", EncoderBissC_commands::errors, "CRC error count",CMDFLAG_GET);
	registerCommand("dir", EncoderBissC_commands::direction, "Invert direction",CMDFLAG_GET|CMDFLAG_SET);
}

CommandStatus EncoderBissC::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<EncoderBissC_commands>(cmd.cmdId)){
	case EncoderBissC_commands::bits:
		handleGetSet(cmd, replies, this->lenghtDataBit);
		if(this->lenghtDataBit > 24 && this->spiSpeed >= 3){
			this->spiSpeed = 2; // Drop SPI speed to prevent encoder from losing data.
		}
		break;
	case EncoderBissC_commands::errors:
		replies.emplace_back(numErrors);
		break;
	case EncoderBissC_commands::speed:
		handleGetSet(cmd, replies, this->spiSpeed);
		this->spiSpeed = clip(this->spiSpeed,1,3); // Limit from 1-3
		if(cmd.type == CMDtype::set){
			configSPI();
		}
		break;

	case EncoderBissC_commands::direction:
		handleGetSet(cmd, replies, this->invertDirection);
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;

}
