/*
 * PCF8574.cpp
 *
 *  Created on: 10.03.2022
 *      Author: Yannick
 */

#include "PCF8574.h"
#include "math.h"

PCF8574::PCF8574(I2CPort &port) : port(port) {
	config.ClockSpeed = 100000;
	config.DutyCycle = I2C_DUTYCYCLE_2;
	config.OwnAddress1 = 0;
	config.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	config.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	config.OwnAddress2 = 0;
	config.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	config.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	port.configurePort(&config);
}

PCF8574::~PCF8574() {

}

uint8_t PCF8574::readByte(const uint8_t devAddr){
	uint8_t data = 0;

	port.receiveMaster(devAddr * 2 + 1, &data, 1, 250);
	return data;
}

void PCF8574::writeByte(const uint8_t devAddr,uint8_t data){
	lastWriteData = data;
	port.transmitMaster(devAddr * 2 , &lastWriteData, 1, 250);
}

ClassIdentifier PCF8574Buttons::info = {
		 .name = "I2C PCF8574" ,
		 .id=CLSID_BTN_PCF,
 };
const ClassIdentifier PCF8574Buttons::getInfo(){
	return info;
}


PCF8574Buttons::PCF8574Buttons() : PCF8574(i2cport) , CommandHandler("pcfbtn", CLSID_BTN_PCF, 0) {
	CommandHandler::registerCommands();
	ButtonSource::btnnum=8;
	registerCommand("btnnum", PCF8574Buttons_commands::btnnum, "Amount of buttons",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("invert", PCF8574Buttons_commands::invert, "Invert buttons",CMDFLAG_GET | CMDFLAG_SET);
	restoreFlash();
}

PCF8574Buttons::~PCF8574Buttons() {

}

void PCF8574Buttons::saveFlash(){
	uint16_t conf1 = (btnnum-1) & 0x3F;
	conf1 |= (invert & 0x1) << 6;
	Flash_Write(ADR_PCFBTN_CONF1, conf1);
}

void PCF8574Buttons::restoreFlash(){
	uint16_t conf1;
	if(Flash_Read(ADR_PCFBTN_CONF1, &conf1)){
		setBtnNum((conf1 & 0x3F) +1);
		invert = (conf1 >> 6) & 0x1;
	}
}

uint8_t PCF8574Buttons::readButtons(uint64_t* buf){
	if(!port.isTaken()){
		port.takeSemaphore();
		lastButtons = 0;
		for(uint8_t idx = 0; idx < numBytes; idx++){

			uint8_t dat = readByte(0x20+idx);
			if(invert){
				dat = ~dat;
			}
			lastButtons |= dat << (idx*8);
		}
		port.giveSemaphore();
	}
	lastButtons &= mask;
	*buf = lastButtons;
	return btnnum;
}


uint16_t PCF8574Buttons::getBtnNum(){
	// Amount of readable buttons
	return btnnum;
}


void PCF8574Buttons::setBtnNum(uint8_t num){
	num = clip<uint8_t,uint8_t>(num, 1, 64); // up to 8 PCF8574 can be chained resulting in 64 buttons
	this->btnnum = num;
	this->numBytes = 1+((num-1)/8);

	mask = pow(2,num)-1;
}

CommandStatus PCF8574Buttons::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<PCF8574Buttons_commands>(cmd.cmdId)){

	case PCF8574Buttons_commands::btnnum:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->btnnum));
		}else if(cmd.type == CMDtype::set){
			setBtnNum(cmd.val);
		}
	break;
	case PCF8574Buttons_commands::invert:
		return handleGetSet(cmd, replies, this->invert);
	break;

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}
