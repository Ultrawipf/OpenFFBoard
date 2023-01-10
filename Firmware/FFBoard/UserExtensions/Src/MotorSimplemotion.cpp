/*
 * MotorSimplemotion.cpp
 *
 *  Created on: Jan 9, 2023
 *      Author: Yannick
 */

#include "target_constants.h"
#ifdef SIMPLEMOTION
#include "MotorSimplemotion.h"
#include "CRC.h"

bool MotorSimplemotion::crcTableInitialized = false;
std::array<uint8_t,256> MotorSimplemotion::tableCRC8 __attribute__((section (".ccmram")));
std::array<uint16_t,256> MotorSimplemotion::tableCRC16 __attribute__((section (".ccmram")));

bool MotorSimplemotion1::inUse = false;
ClassIdentifier MotorSimplemotion1::info = {
	 .name = "Simplemotion 1" ,
	 .id=CLSID_MOT_SM1
};
bool MotorSimplemotion2::inUse = false;
ClassIdentifier MotorSimplemotion2::info = {
	 .name = "Simplemotion 2" ,
	 .id=CLSID_MOT_SM2
};

MotorSimplemotion::MotorSimplemotion(uint8_t instance) : CommandHandler("sm", CLSID_MOT_SM1, address), address(address+1){
	//Init CRC table at runtime to save flash
	if(!crcTableInitialized){
		makeCrcTable(tableCRC8, crcpoly, 8); // Generate a CRC8 table the first time an instance is created
		makeCrcTable(tableCRC16, crcpoly16, 16,true,true); // Make a CRC16 table
		crcTableInitialized = true;
	}

}

MotorSimplemotion::~MotorSimplemotion() {
	// TODO Auto-generated destructor stub
}


/**
 * Sends a fast cycle packet. Driver will reply with status and position
 */
void MotorSimplemotion::sendFastUpdate(uint16_t val1,uint16_t val2){
	uint8_t buffer[6] = {SMCMD_FAST_UPDATE_CYCLE,
			(uint8_t)(val1 & 0xff),(uint8_t)((val1 >> 8) & 0xff),
			(uint8_t)(val2 & 0xff),(uint8_t)((val2 >> 8) & 0xff),
			0};
	buffer[5] = calculateCrc8(tableCRC8,buffer,5,0x52);
	uartport->transmit_IT((char*)(buffer), 6); // Send update
	uartport->receiveIT((char*)(rxbuf), 6); // Receive reply
}

void MotorSimplemotion::turn(int16_t power){
	sendFastUpdate((uint16_t)power, 0);
}

Encoder* MotorSimplemotion::getEncoder(){
	return static_cast<Encoder*>(this);
}

uint32_t MotorSimplemotion::getCpr(){
	return 0xffff;
}

/**
 * In order to get a position update the fast update must be sent first by updating a torque value
 */
int32_t MotorSimplemotion::getPos(){
	return 0;
}

void MotorSimplemotion::setPos(int32_t pos){

}
void MotorSimplemotion::saveFlash(){

}

void MotorSimplemotion::restoreFlash(){

}

void MotorSimplemotion::uartRcv(char& buf){

}

EncoderType MotorSimplemotion::getEncoderType(){
		return EncoderType::absolute;
}

void MotorSimplemotion::startUartTransfer(UARTPort* port,bool transmit){
	port->takeSemaphore();
	if(transmit)
		writeEnablePin.set();
}
void MotorSimplemotion::endUartTransfer(UARTPort* port,bool transmit){
	// Disable write pin
	if(transmit)
		writeEnablePin.reset();
	port->giveSemaphore();
}

void MotorSimplemotion::registerCommands(){
	CommandHandler::registerCommands();

//	registerCommand("errors", MotorSimplemotion_commands::errors, "CRC error count",CMDFLAG_GET);
}

CommandStatus MotorSimplemotion::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<MotorSimplemotion_commands>(cmd.cmdId)){

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;

}

#endif
