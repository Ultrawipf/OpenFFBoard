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
#include "cpp_target_config.h"

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

MotorSimplemotion::MotorSimplemotion(uint8_t instance) : CommandHandler("sm", CLSID_MOT_SM1, address),UARTDevice(motor_uart), address(address+1){
	//Init CRC table at runtime to save flash
	if(!crcTableInitialized){
		makeCrcTable(tableCRC8, crcpoly, 8); // Generate a CRC8 table the first time an instance is created
		makeCrcTable(tableCRC16, crcpoly16, 16,true,true); // Make a CRC16 table
		crcTableInitialized = true;
	}
	cpr = 8192;

	// Set up uart port
	UART_InitTypeDef uartconf;
	uartconf.BaudRate = 460800;
	uartconf.WordLength = UART_WORDLENGTH_8B;
	uartconf.StopBits = UART_STOPBITS_1;
	uartconf.Parity = UART_PARITY_NONE;
	uartconf.Mode = UART_MODE_TX_RX;
	uartconf.HwFlowCtl = UART_HWCONTROL_NONE;
	uartconf.OverSampling = UART_OVERSAMPLING_8;
	uartport->reconfigurePort(uartconf);
	writeEnablePin.reset();

	registerCommands();
}

MotorSimplemotion::~MotorSimplemotion() {
	// TODO Auto-generated destructor stub
}


/**
 * Sends a fast cycle packet. Driver will reply with status and position
 */
void MotorSimplemotion::sendFastUpdate(uint16_t val1,uint16_t val2){
	if(uartport->isTaken() || waitingFastUpdate){
		//pulseErrLed();
		if(HAL_GetTick()-lastSentTime>10){
			uartport->abortReceive();
			resetBuffer();
			waitingFastUpdate = false;
			pulseClipLed();
		}
		return;
	}

	fastbuffer.header = SMCMD_FAST_UPDATE_CYCLE;
	fastbuffer.adr = this->address;
	fastbuffer.val1 = val1;
	fastbuffer.val2 = val2;
	fastbuffer.crc = calculateCrc8(tableCRC8,(uint8_t*)&fastbuffer,6,crc8init);

	if(!uartport->transmit_IT((char*)(&fastbuffer), 7)) // Send update
	{
		pulseErrLed();
		return;
	}

	lastSentTime = HAL_GetTick();
	waitingFastUpdate = true;
	uartport->registerInterrupt(); // Wait for reply data

}

void MotorSimplemotion::turn(int16_t power){
	lastTorque = power;
	sendFastUpdate((uint16_t)power, 0);
}

Encoder* MotorSimplemotion::getEncoder(){
	return static_cast<Encoder*>(this);
}


/**
 * In order to get a position update the fast update must be sent first by updating a torque value
 */
int32_t MotorSimplemotion::getPos(){
	if(HAL_GetTick()-lastUpdateTime>100){
		this->turn(lastTorque); // Sending torque updates the position
	}
	return position;
}

void MotorSimplemotion::setPos(int32_t pos){

}
void MotorSimplemotion::saveFlash(){

}

void MotorSimplemotion::restoreFlash(){

}

void MotorSimplemotion::updatePosition(uint16_t value){
	// Position is in raw encoder counts but overflows between 0 and 0xffff.
	int32_t diff = value-lastPosRep;
	if(abs(diff) > 0x7fff){
		// Overflow likely
		if(value < 0x7fff)
			overflows += 1;
		else if(value >= 0x7fff)
			overflows -= 1;
	}
	position = value + (0xffff * overflows);
	lastPosRep = value;
	lastUpdateTime = HAL_GetTick();
}

void MotorSimplemotion::updateStatus(uint16_t value){

}

void MotorSimplemotion::sendCommand(uint8_t len,uint8_t* buf){
	uint8_t cmdbuffer[len+5];
	cmdbuffer[0] = SMCMD_INSTANT_CMD;
	cmdbuffer[1] = len;
	cmdbuffer[2] = address;
	memcpy(cmdbuffer+3,buf,len);
	uint16_t crc = calculateCrc16_8(tableCRC16,cmdbuffer,len+3,0);
	cmdbuffer[3+len] = (crc << 8) & 0xff;
	cmdbuffer[4+len] = (crc) & 0xff;
	uartport->transmit_IT((char*)(cmdbuffer), len+5); // Send update
	// TODO receive reply...
}

void MotorSimplemotion::resetBuffer(){
	memset((char*)rxbuf,0,RXBUF_SIZE);
	rxbuf_i = 0;
}

void MotorSimplemotion::uartRcv(char& buf){
	uint32_t errorcodes = uartport->getErrors();
	if(errorcodes){
		// Flush buffer if error occured in case of noise
		resetBuffer();
		return;
	}
	// Append to buffer while not overrun
	if(rxbuf_i < RXBUF_SIZE){
		rxbuf[rxbuf_i++] = buf;
	}else{
		// Overrun
		resetBuffer();
		pulseErrLed();
		return;
	}

	// Check if we can parse a command
	char byte1 = rxbuf[0];

	if(waitingFastUpdate && byte1 == SMCMD_FAST_UPDATE_CYCLE_RET && rxbuf_i == 6) // Fast update reply
	{
		// We know the size of the fast update
		waitingFastUpdate = false;

		Sm2FastUpdate_reply packet;
		memcpy(&packet,(char*)rxbuf,6);

		if(calculateCrc8(tableCRC8, (uint8_t*)rxbuf, 6, crc8init) != 0){
			crcerrors++;
			resetBuffer();
			return;
		}
		updatePosition(packet.val1);
		updateStatus(packet.val2);
		resetBuffer();
		return;
	}
	else if(byte1 == SMCMD_INSTANT_CMD_RET) // Standard reply
	{

	}
	else
	{
		uartport->registerInterrupt(); // Wait for next byte if nothing can be parsed yet
	}

}

void MotorSimplemotion::startMotor(){

}

void MotorSimplemotion::stopMotor(){
	turn(0);
}

EncoderType MotorSimplemotion::getEncoderType(){
		return EncoderType::absolute;
}

void MotorSimplemotion::startUartTransfer(UARTPort* port,bool transmit){
	port->takeSemaphore(transmit);
	if(transmit){
		writeEnablePin.set();
	}
}
void MotorSimplemotion::endUartTransfer(UARTPort* port,bool transmit){
	// Disable write pin
	if(transmit){
		writeEnablePin.reset();
	}
	port->giveSemaphore(transmit);
}

void MotorSimplemotion::registerCommands(){
	CommandHandler::registerCommands();

	registerCommand("errors", MotorSimplemotion_commands::errors, "CRC error count",CMDFLAG_GET);
}

CommandStatus MotorSimplemotion::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<MotorSimplemotion_commands>(cmd.cmdId)){
	case MotorSimplemotion_commands::errors:
		replies.emplace_back(this->crcerrors);
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;

}

#endif
