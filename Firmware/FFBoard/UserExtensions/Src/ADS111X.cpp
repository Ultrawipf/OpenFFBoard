/*
 * ADS111X.cpp
 *
 *  Created on: 21.04.2022
 *      Author: Yannick
 */

#include "ADS111X.h"
#ifdef I2C_PORT
ADS111X::ADS111X(I2CPort &port,uint8_t address) : port(port), address(address) {
	port.takePort();

}

ADS111X::~ADS111X() {
	port.freePort();
}


void ADS111X::readRegIT(const uint8_t reg,uint16_t* data){
	port.takeSemaphore();
	port.readMemIT(this, address, reg, 1, (uint8_t*)data, 2);
//	port.transmitMaster(this, address, (uint8_t*)&reg, 1, 100);
//	port.receiveMasterIT(this, address, (uint8_t*)data, 2);
}

uint16_t ADS111X::readReg(const uint8_t reg){
	uint16_t buf=0;
	port.takeSemaphore();
	port.readMem(this, address, reg, 1, (uint8_t*)&buf, 2,100);
	return buf;
}

void ADS111X::writeRegIT(const uint8_t reg,uint16_t data){
	writeItBuffer = data;
	port.takeSemaphore();
//	port.transmitMaster(this, address, (uint8_t*)&reg, 1, 100);
//	port.transmitMasterIT(this, address, (uint8_t*)&data, 2);
	port.writeMemIT(this, address, reg, 1, (uint8_t*)&writeItBuffer, 2);
}

/**
 * Changes PGA gain and input range
 * 0 = 2/3
 * 1 = 1
 * 2 = 2
 * 3 = 4
 * 4 = 8
 * 5 = 16
 */
void ADS111X::setGain(uint16_t gain){
	gain = clip<uint16_t,uint16_t>(gain, 0, 5);
	this->registers.config &= ~0x0E00;
	this->registers.config |= gain << 9;
	writeRegIT(0x01, this->registers.config);
}

uint16_t ADS111X::getGain(){
	this->registers.config = readReg(0x01);
	return (this->registers.config & 0x0E00) >> 9;
}

/**
 * Changes samplerate
 * 0 = 8 sps
 * 1 = 16 sps
 * 2 = 32 sps
 * 3 = 34 sps
 * 4 = 128 sps
 * 5 = 250 sps
 * 6 = 475 sps
 * 7 = 860 sps
 */
void ADS111X::setDatarate(uint16_t rate){
	datarate = clip<uint16_t>(rate, 0, 8);
	this->registers.config &= ~0xE0;
	this->registers.config |= datarate << 5;
	writeRegIT(0x01, this->registers.config);
}

uint16_t ADS111X::getDatarate(){
	return datarate;
}

/**
 * Channel:
 * differential = false
 * 0: 0=p, gnd=n
 * 1: 1=p, gnd=n
 * 2: 2=p, gnd=n
 * 3: 3=p, gnd=n
 *
 * differential = true
 * 0: 0=p, 1=n
 * 1: 1=p, 3=n
 * 2: 2=p, 3=n
 * 3: 0=p, 3=n
 */
void ADS111X::startConversion(uint8_t channel, bool differential){
	uint16_t mode = 0;
	if(differential){
		switch(channel)
		{
			default: case 0: mode = 0x0000; break;
			case 1: mode = 0x2000; break;
			case 2: mode = 0x3000; break;
			case 3: mode = 0x1000; break;
		}
	}else{
		switch(channel)
		{
			default: case 0: mode = 0x4000; break;
			case 1: mode = 0x5000; break;
			case 2: mode = 0x6000; break;
			case 3: mode = 0x7000; break;
		}
	}

	this->registers.config &= ~mode;
	this->registers.config |= mode;
	this->registers.config |= 0x8000; // Start conversion
	this->registers.config |= datarate << 5; // Read datarate from separate variable to make sure it is updated if connection was interrupted

	writeRegIT(0x01, this->registers.config);
}



void ADS111X::startI2CTransfer(I2CPort* port){
	//port->takeSemaphore(); // Take semaphore before setting buffers
}

void ADS111X::endI2CTransfer(I2CPort* port){
	port->giveSemaphore(); // Take semaphore before setting buffers
}




// ---------------------------------------------------------------------- //
#ifdef ADS111XANALOG
ClassIdentifier ADS111X_AnalogSource::info = {
		 .name = "ADS111X analog" ,
		 .id=CLSID_ANALOG_ADS111X,
 };
const ClassIdentifier ADS111X_AnalogSource::getInfo(){
	return info;
}

ADS111X_AnalogSource::ADS111X_AnalogSource() : ADS111X(i2cport) , CommandHandler("adsAnalog", CLSID_ANALOG_ADS111X, 0), Thread("ads111x", 256, 25) {
	CommandHandler::registerCommands();
	registerCommand("inputs", ADS111X_AnalogSource_commands::axes, "Amount of inputs (1-4)",CMDFLAG_GET | CMDFLAG_SET);
	//registerCommand("addr", ADS111X_AnalogSource_commands::address, "Address",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("gain", ADS111X_AnalogSource_commands::gain, "Gain mode",CMDFLAG_GET | CMDFLAG_SET);
	restoreFlash();
	//initialize();
	this->Start();
}

ADS111X_AnalogSource::~ADS111X_AnalogSource(){

}

void ADS111X_AnalogSource::saveFlash(){

}

void ADS111X_AnalogSource::restoreFlash(){
	// Restore gain
	// Restore axes
	// Restore address
}

void ADS111X_AnalogSource::initialize(){
	setDatarate(8); // Max rate

	state = ADS111X_AnalogSource_state::idle;
}

// Handles starting next transfer for >1 byte transfers
void ADS111X_AnalogSource::Run(){
	while(true){
		WaitForNotification();

		/*
		 * Sequence:
		 * Start sampling at channel 0
		 * Wait until request done
		 * Poll if sampling is done
		 * Wait until request done and possibly repeat polling
		 * Read sample and store in buffer when request done
		 *
		 */

		if(lastAxis+1 < this->axes){
			// Next transfer
			if(state == ADS111X_AnalogSource_state::reading){
				state = ADS111X_AnalogSource_state::sampling;
				ADS111X::startConversion(++lastAxis); // Change channel and sample

			}else if(state == ADS111X_AnalogSource_state::sampling){
				// Check if done
				Delay(1); // Sampling takes at least 1ms so we sleep before polling
				ADS111X::readRegIT(0x01, &registers.config);

			}else if(state == ADS111X_AnalogSource_state::getsample){

				// if done read sample
				state = ADS111X_AnalogSource_state::reading; // Read next
				ADS111X::readRegIT(0x00, &sampleBuffer); // Read last conversion
				//ADS111X::readRegIT(0x00, &sampleBuffer); // Read last conversion
			}

		}else{
			lastAxis = 0;
			//readingData = false; // Done
			state = ADS111X_AnalogSource_state::idle;
		}
	}
}

// i2c complete interrupt signals thread to start next transfer
void ADS111X_AnalogSource::i2cRxCompleted(I2CPort* port){
	if(port != &this->port || state == ADS111X_AnalogSource_state::idle){
		return;
	}
	if(state == ADS111X_AnalogSource_state::sampling){
		// Check config reg if conversion is done
		if((registers.config & 0x8000) == 0){
			// Done
			state = ADS111X_AnalogSource_state::getsample;
		}
	}else if(state == ADS111X_AnalogSource_state::getsample){
		this->buf[lastAxis] = sampleBuffer; // Store i2c buffer in axis buffer
	}

	lastSuccess = HAL_GetTick();
	NotifyFromISR();

}


std::vector<int32_t>* ADS111X_AnalogSource::getAxes(){
	if(state == ADS111X_AnalogSource_state::idle){
		// Begin transfer
		lastAxis = 0;
		//Notify();
		//readingData = true;
		state = ADS111X_AnalogSource_state::sampling;
		ADS111X::startConversion(0);
	}
	return &buf;
}

CommandStatus ADS111X_AnalogSource::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<ADS111X_AnalogSource_commands>(cmd.cmdId)){

	case ADS111X_AnalogSource_commands::axes:
		if(cmd.type == CMDtype::set){
			axes = clip(cmd.val,1,4);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(axes);
		}
	break;

	case ADS111X_AnalogSource_commands::gain:
		if(cmd.type == CMDtype::set){
			this->setGain(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(this->getGain());
		}
	break;


	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}
#endif
#endif
