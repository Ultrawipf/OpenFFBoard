/*
 * ADS111X.cpp
 *
 *  Created on: 21.04.2022
 *      Author: Yannick
 */

#include "ADS111X.h"

ADS111X::ADS111X(I2CPort &port,uint8_t address) : port(port), address(address) {
	port.takePort();

}

ADS111X::~ADS111X() {
	port.freePort();
}


void ADS111X::readRegIT(const uint8_t reg,uint8_t* data,uint8_t regsize){
	port.readMemIT(this, address, reg, 8, data, regsize);
}

ClassIdentifier ADS111X_AnalogSource::info = {
		 .name = "ADS111X analog" ,
		 .id=CLSID_ANALOG_ADS111X,
 };
const ClassIdentifier ADS111X_AnalogSource::getInfo(){
	return info;
}

ADS111X_AnalogSource::ADS111X_AnalogSource() : ADS111X(i2cport) , CommandHandler("adsAnalog", CLSID_ANALOG_ADS111X, 0), Thread("ads111x", 64, 25) {
	CommandHandler::registerCommands();
	//registerCommand("inputs", ADS111X_AnalogSource_commands::axes, "Enabled inputs",CMDFLAG_GET | CMDFLAG_SET);

	restoreFlash();

	this->Start();
}

void ADS111X_AnalogSource::saveFlash(){

}
void ADS111X_AnalogSource::restoreFlash(){

}

// Handles starting next transfer for >1 byte transfers
void ADS111X_AnalogSource::Run(){
	while(true){
		WaitForNotification();
		if(lastByte+1 < this->numBytes){
			// Next transfer

		}else{
			lastByte = 0;
			readingData = false; // Done
//			lastButtons = currentButtons;
		}
	}
}

// i2c complete interrupt signals thread to start next transfer
void ADS111X_AnalogSource::i2cRxCompleted(I2CPort* port){
	if(port != &this->port || !readingData){
		return;
	}
	lastSuccess = HAL_GetTick();

	NotifyFromISR();

}


std::vector<int32_t>* ADS111X_AnalogSource::getAxes(){
	if(!readingData){
		// Begin transfer
	}
	return &buf;
}
