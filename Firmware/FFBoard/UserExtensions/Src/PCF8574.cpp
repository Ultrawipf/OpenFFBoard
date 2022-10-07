/*
 * PCF8574.cpp
 *
 *  Created on: 10.03.2022
 *      Author: Yannick
 */


#include "PCF8574.h"
#include "math.h"
#ifdef I2C_PORT
PCF8574::PCF8574(I2CPort &port) : port(port) {

	port.takePort(this);
}

PCF8574::~PCF8574() {
	port.freePort(this);
}

//void PCF8574::configurePort(bool fastMode){
//	config.ClockSpeed = fastMode ? 400000 : 100000;
//	config.DutyCycle = I2C_DUTYCYCLE_2;
//	config.OwnAddress1 = 0;
//	config.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
//	config.DualAddressMode = I2C_DUALADDRESS_DISABLE;
//	config.OwnAddress2 = 0;
//	config.GeneralCallMode = I2C_GENERALCALL_DISABLE;
//	config.NoStretchMode = I2C_NOSTRETCH_ENABLE;
//	port.configurePort(&config);
//}

uint8_t PCF8574::readByte(const uint8_t devAddr){
	uint8_t data = 0;
	port.receiveMaster(this,devAddr, &data, 1, 250);
	return data;
}

void PCF8574::readByteIT(const uint8_t devAddr,uint8_t* data){
	port.receiveMasterIT(this,devAddr, data, 1);
}

void PCF8574::writeByteIT(const uint8_t devAddr,uint8_t* data){
	port.transmitMasterIT(this,devAddr, data, 1);
}

void PCF8574::writeByte(const uint8_t devAddr,uint8_t data){
	lastWriteData = data;
	port.transmitMaster(this,devAddr , &lastWriteData, 1, 250);
}

//void PCF8574::startI2CTransfer(I2CPort* port){
//	transferActive = true;
//}
//
//void PCF8574::endI2CTransfer(I2CPort* port){
//	transferActive = false;
//}


#ifdef PCF8574BUTTONS
ClassIdentifier PCF8574Buttons::info = {
		 .name = "I2C PCF8574" ,
		 .id=CLSID_BTN_PCF,
 };
const ClassIdentifier PCF8574Buttons::getInfo(){
	return info;
}


PCF8574Buttons::PCF8574Buttons() : PCF8574(i2cport) , CommandHandler("pcfbtn", CLSID_BTN_PCF, 0), Thread("pcfbtn", 64, 20) {
	CommandHandler::registerCommands();
	ButtonSource::btnnum=8;
	registerCommand("btnnum", PCF8574Buttons_commands::btnnum, "Amount of buttons",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("invert", PCF8574Buttons_commands::invert, "Invert buttons",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("speed", PCF8574Buttons_commands::speed, "400kb/s mode",CMDFLAG_GET | CMDFLAG_SET);
	restoreFlash();

	this->Start();
}

PCF8574Buttons::~PCF8574Buttons() {

}

// Handles starting next transfer for >1 byte transfers
void PCF8574Buttons::Run(){
	while(true){
		WaitForNotification();
		if(lastByte+1 < this->numBytes){
			// Next transfer
			lastData = 0;
			readByteIT(0x20+ ++lastByte, &lastData);
		}else{
			lastByte = 0;
			readingData = false; // Done
			lastButtons = currentButtons;
		}
	}
}

// i2c complete interrupt signals thread to start next transfer
void PCF8574Buttons::i2cRxCompleted(I2CPort* port){
	if(port != &this->port || !readingData){
		return;
	}
	lastSuccess = HAL_GetTick();
	currentButtons |= (uint64_t)lastData << (lastByte*8);
	NotifyFromISR();

}

void PCF8574Buttons::saveFlash(){
	uint16_t conf1 = (btnnum-1) & 0x3F;
	conf1 |= (invert & 0x1) << 6;
	//conf1 |= (fastmode & 0x1) << 7;
	Flash_Write(ADR_PCFBTN_CONF1, conf1);
}

void PCF8574Buttons::restoreFlash(){
	uint16_t conf1;
	if(Flash_Read(ADR_PCFBTN_CONF1, &conf1)){
		setBtnNum((conf1 & 0x3F) +1);
		invert = (conf1 >> 6) & 0x1;
		//fastmode = (conf1 >> 7) & 0x1;
	}
}


void PCF8574Buttons::i2cError(I2CPort* port){
	readingData = false;
	port->resetPort();
	lastSuccess = HAL_GetTick();
}

uint8_t PCF8574Buttons::readButtons(uint64_t* buf){

	if(invert){
		*buf = ~lastButtons;
	}else{
		*buf = lastButtons;
	}
	*buf &= mask;


	// Update
	if(HAL_GetTick()-lastSuccess > timeout && readingData){
		readingData = false;
		port.resetPort();
		lastSuccess = HAL_GetTick();
	}
	if(!port.isTaken() && !readingData){
		readingData = true;
		//lastButtons = 0;
		currentButtons = 0;
		lastByte = 0;
		readByteIT(0x20, &lastData); // Read first address
	}
	return btnnum;
}


uint16_t PCF8574Buttons::getBtnNum(){
	// Amount of readable buttons
	return btnnum;
}

/**
 * Changes the amount of buttons to be read
 * num/8 chips required. Each of them taking 200µS for a transfer!
 * Not recommended to use more than 4 modules
 */
void PCF8574Buttons::setBtnNum(uint8_t num){
	num = clip<uint8_t,uint8_t>(num, 1, 64); // up to 8 PCF8574 can be chained resulting in 64 buttons
	this->btnnum = num;
	this->numBytes = 1+((num-1)/8);

	if(num == 64){ // Special case
		mask = 0xffffffffffffffff;
	}else{
		mask = (uint64_t)pow<uint64_t>(2,num)-(uint64_t)1; // Must be done completely in 64 bit!
	}
	port.resetPort();
}

CommandStatus PCF8574Buttons::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<PCF8574Buttons_commands>(cmd.cmdId)){

	case PCF8574Buttons_commands::btnnum:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->btnnum);
		}else if(cmd.type == CMDtype::set){
			setBtnNum(cmd.val);
		}
	break;
	case PCF8574Buttons_commands::invert:
		return handleGetSet(cmd, replies, this->invert);
	break;

	case PCF8574Buttons_commands::speed:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->port.getSpeedPreset());
		}else if(cmd.type == CMDtype::set){
			port.setSpeedPreset(cmd.val);
		}
	break;


	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}
#endif
#endif //i2c
