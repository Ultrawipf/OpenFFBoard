/*
 * ADS111X.cpp
 *
 *  Created on: 21.04.2022
 *      Author: Yannick
 */

#include "ADS111X.h"
#ifdef I2C_PORT
ADS111X::ADS111X(I2CPort &port,uint8_t address) : port(port), address(address) {
	port.takePort(this);
}

ADS111X::~ADS111X() {
	port.freePort(this);
}


void ADS111X::readRegIT(const uint8_t reg,uint16_t* data){
	port.takeSemaphore();
	port.readMemIT(this, address, reg, 1, (uint8_t*)data, 2);
}

uint16_t ADS111X::readReg(const uint8_t reg){
	uint16_t buf=0;
	port.takeSemaphore();
	port.readMem(this, address, reg, 1, (uint8_t*)&buf, 2,100);
	return buf;
}

void ADS111X::writeRegIT(const uint8_t reg,uint16_t data){
	writeItBuffer = __REV16(data);
	port.takeSemaphore();

	port.writeMemIT(this, address, reg, 1, (uint8_t*)&writeItBuffer, 2);
}

void ADS111X::writeReg(const uint8_t reg,uint16_t data){
	writeItBuffer = __REV16(data);
	port.takeSemaphore();
	port.writeMem(this, address, reg, 1, (uint8_t*)&writeItBuffer, 2,100);
}

void ADS111X::setThresh(uint16_t lowTh, uint16_t highTh){
	registers.lothresh = lowTh;
	registers.hithresh = highTh;
	writeRegIT(0x02, registers.lothresh);
	writeRegIT(0x03, registers.hithresh);
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
	this->gain = clip<uint16_t,uint16_t>(gain, 0, 5);
//	this->registers.config &= ~0x0E00;
//	this->registers.config |= this->gain << 9;
//	writeRegIT(0x01, this->registers.config);
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
	datarate = clip<uint16_t,uint16_t>(rate, 0, 7);
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
 * 1: 2=p, 3=n
 * 2: 0=p, 3=n
 * 3: 1=p, 3=n
 */
void ADS111X::startConversion(uint8_t channel, bool differential){
	uint16_t mux = 0;
	if(differential){
		switch(channel)
		{
			default:
			case 0:
				mux = 0x0000;
				break;
			case 1:
				mux = 0x3000;
				break;
			case 2:
				mux = 0x1000;
				break;
			case 3:
				mux = 0x2000;
				break;

		}
	}else{
		switch(channel)
		{
			default:
			case 0:
				mux = 0x4000;
				break;
			case 1:
				mux = 0x5000;
				break;
			case 2:
				mux = 0x6000;
				break;
			case 3:
				mux = 0x7000;
				break;

		}
	}
	this->registers.config = 0x103; // Default settings. Single conversion 103
	this->registers.config |= mux;
	this->registers.config |= 0x8000; // Start conversion
	this->registers.config |= datarate << 5; // Read datarate from separate variable to make sure it is updated if connection was interrupted
	this->registers.config |= gain << 9;
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
		 .name = "ADS111X ADC" ,
		 .id=CLSID_ANALOG_ADS111X,
 };
const ClassIdentifier ADS111X_AnalogSource::getInfo(){
	return info;
}


const std::array<std::pair<uint16_t,uint16_t>,4> minMaxValAddr = {
	std::pair<uint16_t,uint16_t>(ADR_ADS111X_MIN_0,ADR_ADS111X_MAX_0),
	std::pair<uint16_t,uint16_t>(ADR_ADS111X_MIN_1,ADR_ADS111X_MAX_1),
	std::pair<uint16_t,uint16_t>(ADR_ADS111X_MIN_2,ADR_ADS111X_MAX_2),
	std::pair<uint16_t,uint16_t>(ADR_ADS111X_MIN_3,ADR_ADS111X_MAX_3),

};

ADS111X_AnalogSource::ADS111X_AnalogSource() : ADS111X(i2cport) , CommandHandler("adsAnalog", CLSID_ANALOG_ADS111X, 0),AnalogAxisProcessing(4,this,this, true,true,true,true), Thread("ads111x", 64, 25) {
	CommandHandler::registerCommands();
	setAxes(axes,differentialMode);
	registerCommand("inputs", ADS111X_AnalogSource_commands::axes, "Amount of inputs (1-4 or 1-2 if differential)",CMDFLAG_GET | CMDFLAG_SET);
	//registerCommand("addr", ADS111X_AnalogSource_commands::address, "Address",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("diff", ADS111X_AnalogSource_commands::differential, "Differential mode (Ch0= 0p 1n Ch1= 2p 3n)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("gain", ADS111X_AnalogSource_commands::gain, "PGA scale (0-5)",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("rate", ADS111X_AnalogSource_commands::rate, "Data rate (0-7)",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	restoreFlash();
	this->Start();
}

ADS111X_AnalogSource::~ADS111X_AnalogSource(){

}

void ADS111X_AnalogSource::saveFlash(){
	AnalogAxisProcessing::saveMinMaxValues(minMaxValAddr);
	uint16_t data = axes & 0x7;
	data |= (gain & 0x7) << 3;
	data |= (datarate & 0x7) << 6;
	data |= (differentialMode) << 9;
	data |= AnalogAxisProcessing::encodeAnalogProcessingConfToInt(AnalogAxisProcessing::getAnalogProcessingConfig()) << 10;
	Flash_Write(ADR_ADS111X_CONF1, data);
}

void ADS111X_AnalogSource::restoreFlash(){
	AnalogAxisProcessing::restoreMinMaxValues(minMaxValAddr);
	uint16_t data;
	if(Flash_Read(ADR_ADS111X_CONF1, &data)){
		setAxes(data & 0x7, ((data >> 9) & 0x1) != 0);
		setGain((data >> 3) & 0x7);
		setDatarate((data >> 6) & 0x7);
		AnalogAxisProcessing::setAnalogProcessingConfig(AnalogAxisProcessing::decodeAnalogProcessingConfFromInt(data >> 10));
	}
}

void ADS111X_AnalogSource::initialize(){
	// Must not be called in constructor
	//setThresh(0,0x8000);
	lastSuccess = HAL_GetTick();
	port.resetPort();
	lastAxis = 0;
	state = ADS111X_AnalogSource_state::idle;
}

void ADS111X_AnalogSource::i2cError(I2CPort* port){
	state = ADS111X_AnalogSource_state::idle;
	port->resetPort();
}

// Handles starting next transfer for >1 byte transfers
void ADS111X_AnalogSource::Run(){
	while(true){


		/*
		 * Sequence:
		 * Start sampling at channel 0
		 * Wait until request done
		 * Poll if sampling is done
		 * Wait until request done and possibly repeat polling
		 * Read sample and store in buffer when request done
		 *
		 */

		if(state == ADS111X_AnalogSource_state::none){
			initialize();
		}

		WaitForNotification();

		if(lastAxis < this->axes){
			// Next transfer
			if(state == ADS111X_AnalogSource_state::beginSampling){
				ADS111X::startConversion(lastAxis,differentialMode); // Change channel and sample

			}else if(state == ADS111X_AnalogSource_state::sampling){
				// Check if done
				Delay(1); // Sampling takes at least 1ms so we sleep before polling
				ADS111X::readRegIT(0x01, &configRegBuf);

			}else if(state == ADS111X_AnalogSource_state::getsample){

				// if done read sample
				state = ADS111X_AnalogSource_state::readingSample; // Read next
				ADS111X::readRegIT(0x00, (uint16_t*)&sampleBuffer); // Read last conversion
			}

		}else{
			// Last conversion done. store in buffer
			lastAxis = 0;
			lastSuccess = HAL_GetTick();

			state = ADS111X_AnalogSource_state::idle;

		}
	}
}

/** i2c complete interrupt signals thread to start next transfer
 * Called when reading the status or a sample
 */
void ADS111X_AnalogSource::i2cRxCompleted(I2CPort* port){
	if(port != &this->port || state == ADS111X_AnalogSource_state::idle){
		return;
	}
	if(state == ADS111X_AnalogSource_state::sampling){
		// Check config reg if conversion is done yet
		configRegBuf = __REV16(configRegBuf);
		if((configRegBuf & 0x8000) != 0){
			// Done
			state = ADS111X_AnalogSource_state::getsample;
		}
	}else if(state == ADS111X_AnalogSource_state::readingSample){
		sampleBuffer = __REV16(sampleBuffer); // Reverse buffer
		if(!differentialMode){
			sampleBuffer = (clip<int16_t,int16_t>(sampleBuffer,0,0x7fff) - 0x3fff) * 2 - 1; // Shift because we can't go below GND anyways
		}
		rawbuf[lastAxis] = (int16_t)sampleBuffer;
		lastAxis++;
		state = ADS111X_AnalogSource_state::beginSampling; // Begin next sample or go idle
	}

	NotifyFromISR();
}
/**
 * Called when a conversion is started or data sent to the adc
 */
void ADS111X_AnalogSource::i2cTxCompleted(I2CPort* port){
	if(port != &this->port || state == ADS111X_AnalogSource_state::idle){
		return;
	}

	if(state == ADS111X_AnalogSource_state::beginSampling){ // Last command started the sampling and was sent out. We are now waiting
		state = ADS111X_AnalogSource_state::sampling;
	}

	NotifyFromISR();
}

std::vector<int32_t>* ADS111X_AnalogSource::getAxes(){

	if(state == ADS111X_AnalogSource_state::idle && !port.isTaken()){
		//Begin transfer
		lastAxis = 0;
		state = ADS111X_AnalogSource_state::beginSampling;
		Notify();
	}

	if(HAL_GetTick() - lastSuccess > 4000){
		state = ADS111X_AnalogSource_state::none;
		Notify();
	}
	buf = rawbuf;
	AnalogAxisProcessing::processAxes(buf); // Do processing every call to keep filter samplerate steady
	return &this->buf;
}

void ADS111X_AnalogSource::setAxes(uint8_t axes,bool differential){
	differentialMode = differential;
	this->axes = clip<uint16_t,uint16_t>(axes,1,differentialMode ? 2 : 4);
	this->buf.resize(this->axes,0);
	this->rawbuf.resize(this->axes,0);
}

CommandStatus ADS111X_AnalogSource::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<ADS111X_AnalogSource_commands>(cmd.cmdId)){

	case ADS111X_AnalogSource_commands::axes:
		if(cmd.type == CMDtype::set){
			setAxes(cmd.val,differentialMode);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(axes);
		}
	break;

	case ADS111X_AnalogSource_commands::gain:
		if(cmd.type == CMDtype::set){
			this->setGain(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(gain);
		}else if(cmd.type == CMDtype::info){
			//replies.emplace_back("2/3x:0,1x:1,2x:2,4x:3,8x:4,16x:5");
			replies.emplace_back("6.144V:0,4.096V:1,2.048V:2,1.024V:3,0.512V:4,0.256V:5");
		}
	break;

	case ADS111X_AnalogSource_commands::differential:
		if(cmd.type == CMDtype::set){
			setAxes(axes,cmd.val != 0); // Limit axes
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(differentialMode);
		}
	break;

	case ADS111X_AnalogSource_commands::rate:
		if(cmd.type == CMDtype::set){
			setDatarate(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(datarate);
		}else if(cmd.type == CMDtype::info){
			replies.emplace_back("8 SPS:0,16 SPS:1,32 SPS:2,64 SPS:3,128 SPS:4,250 SPS:5,475 SPS:6,860 SPS:7");
		}
	break;
	case ADS111X_AnalogSource_commands::address: // No option yet
	default:
		return AnalogAxisProcessing::command(cmd, replies); // Try processing command
	}
	return CommandStatus::OK;
}
#endif
#endif
