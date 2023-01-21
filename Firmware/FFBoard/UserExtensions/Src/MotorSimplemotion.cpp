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
#include "array"

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

MotorSimplemotion::MotorSimplemotion(uint8_t instance) : CommandHandler("sm2", CLSID_MOT_SM1, instance),UARTDevice(motor_uart), address(address+1){
	//Init CRC table at runtime to save flash
	if(!crcTableInitialized){
		makeCrcTable(tableCRC8, crcpoly, 8); // Generate a CRC8 table the first time an instance is created
		makeCrcTable(tableCRC16, crcpoly16, 16,true,true); // Make a CRC16 table
		crcTableInitialized = true;
	}

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
	//getSettings();
}

MotorSimplemotion::~MotorSimplemotion() {

}


/**
 * Sends a fast cycle packet. Driver will reply with status and position
 */
void MotorSimplemotion::sendFastUpdate(uint16_t val1,uint16_t val2){
	// If not initialized try that instead
	if(waitingReply){ // || (uartport->isTaken() && !waitingFastUpdate)
		return; // When we wait for a complex reply we don't start the fast update. Should also wait if port was taken by another class but needs more testing
	}
	if(!initialized){
		if(!getSettings()){
			return;
		}
	}
	if((HAL_GetTick()-lastSentTime>10 && waitingFastUpdate) || (HAL_GetTick() - lastTimeByteReceived > uartErrorTimeout && uartport->isTaken())){
//		uartport->abortReceive();
//		uarterrors++;
		uartErrorOccured = true;
		waitingFastUpdate = false;
//		uartport->giveSemaphore(true); // Force giving semaphore here so we don't cause a block. This may not be ideal
	}

	if(!prepareUartTransmit()){
		uartErrorOccured = true;
		return; // Could not take port
	}
	fastbuffer.header = SMCMD_FAST_UPDATE_CYCLE;
	fastbuffer.adr = this->address;
	fastbuffer.val1 = val1;
	fastbuffer.val2 = val2;
	fastbuffer.crc = calculateCrc8(tableCRC8,(uint8_t*)&fastbuffer,6,crc8init);

	if(!uartport->transmit_IT((char*)(&fastbuffer), 7)) // Send update
	{
		endUartTransfer(uartport, true); // transfer aborted
		uartport->giveSemaphore(true);
		return;
	}
	uartport->registerInterrupt(); // Wait for reply data
	lastSentTime = HAL_GetTick();
	waitingFastUpdate = true;
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
	return position - position_offset;
}

void MotorSimplemotion::setPos(int32_t pos){
	position_offset = position - pos;
}

/**
 * This is requested always before position so we need to make sure we have the cpr
 */
uint32_t MotorSimplemotion::getCpr(){
	if(cpr == 0 || !initialized){
		getSettings();
	}
	return this->cpr;
}

bool MotorSimplemotion::motorReady(){

	return initialized && !hardfault;
}

uint32_t MotorSimplemotion::getCumstat(){
	uint32_t stat = 0;
	if(read1Parameter(MotorSimplemotion_param::cumstat, &stat,MotorSimplemotion_cmdtypes::param32b)){
		set1Parameter(MotorSimplemotion_param::cumstat, 0);
	}
	return stat;
}

bool MotorSimplemotion::getSettings(){
	if(HAL_GetTick() - lastSentTime < 150){
		return false;
	}
	bool status = true;


//	if(getCumstat()){
//		pulseErrLed();
//	}

	uint32_t devtype;
	if(read1Parameter(MotorSimplemotion_param::devtype, &devtype,MotorSimplemotion_cmdtypes::param32b)){
		this->devicetype = devtype;
	}else{
		status = false;
	}

	// Clearfaults
	set1Parameter(MotorSimplemotion_param::faults, 0);


	uint32_t st;
	if(read1Parameter(MotorSimplemotion_param::status, &st,MotorSimplemotion_cmdtypes::param24b)){
		this->status = st;
	}else{
		status = false;
	}
	if(!st){
		return false; // If we can't get a status we have no connection
	}

	uint32_t tcpr = 0;
	if(!read1Parameter(MotorSimplemotion_param::FBR, &tcpr,MotorSimplemotion_cmdtypes::param24b)){
		status=false;
	}

	uint32_t fbd;
	if(read1Parameter(MotorSimplemotion_param::FBD, &fbd,MotorSimplemotion_cmdtypes::param24b)){
		this->encodertype = (MotorSimplemotion_FBR)fbd;
		switch(this->encodertype){
		case MotorSimplemotion_FBR::Serial:
		case MotorSimplemotion_FBR::Hall: // TODO check if this is right for hall, resolver and sincos. Seems like it also must be *4 for serial
		case MotorSimplemotion_FBR::Resolver:
		case MotorSimplemotion_FBR::ABN1:
		case MotorSimplemotion_FBR::ABN2:
			this->cpr = tcpr*4;
			break;

		case MotorSimplemotion_FBR::Sincos16:
			this->cpr = tcpr*16;
			break;
		case MotorSimplemotion_FBR::Sincos64:
			this->cpr = tcpr*64;
			break;
		case MotorSimplemotion_FBR::Sincos256:
			this->cpr = tcpr*256;
			break;
		default:
			ErrorHandler::addError(configError);
			hardfault = true;
			status = false;
		}

	}else{status=false;}

	uint32_t cm; // This is the last transfer with length specified. all future requests with no length will reply in this format
	if(read1Parameter(MotorSimplemotion_param::ControlMode, &cm,MotorSimplemotion_cmdtypes::param24b)){
		if(cm != 3){
			status = false; // control mode must be torque mode
		}
	}else{status=false;}


	initialized = status;
	return status;
}

bool MotorSimplemotion::read1Parameter(MotorSimplemotion_param paramId,uint32_t* reply_p,MotorSimplemotion_cmdtypes replylen){
	std::array<MotorSimplemotion_param,1> paramIds = {paramId};
	std::array<uint32_t*,1> replies = {reply_p};
	return readParameter(paramIds, replies,replylen);
}

bool MotorSimplemotion::set1Parameter(MotorSimplemotion_param paramId,int32_t value,uint32_t* reply_p){
	std::array<std::pair<MotorSimplemotion_param,int32_t>,1> paramIds = {std::make_pair(paramId,value)};
	std::array<uint32_t*,1> replies = {reply_p};
	return writeParameter(paramIds, replies,MotorSimplemotion_cmdtypes::param32b);
}

// In mV
int16_t MotorSimplemotion::getVoltage(){
	uint32_t voltage;
	if(read1Parameter(MotorSimplemotion_param::Voltage, &voltage)){
		return voltage * 10;
	}
	return 0;
}

// Torque in mA
int16_t MotorSimplemotion::getTorque(){
	uint32_t torque_u;
	if(read1Parameter(MotorSimplemotion_param::Torque, &torque_u)){
		return ((int16_t)torque_u * 1000) / 560;
	}
	return 0;
}

void MotorSimplemotion::restart(){
	hardfault = false;
	set1Parameter(MotorSimplemotion_param::systemcontrol, 2, 0);
}

void MotorSimplemotion::updatePosition(uint16_t value){
	// Position is in raw encoder counts but overflows between 0 and 0xffff.
	value += 0x7fff; // Shift half range so we don't immediately overflow and cause a glitch
	int32_t diff = value-lastPosRep;
	if(abs(diff) > 0x7fff){
		// Overflow likely
		if(diff < 0)
			overflows += 1;
		else
			overflows -= 1;
	}
	position = value + (0xffff * overflows) - 0x7fff;
	lastPosRep = value;
	lastUpdateTime = HAL_GetTick();
}

void MotorSimplemotion::updateStatus(uint16_t value){
	status = value;
	lastStatusTime = HAL_GetTick();
	hardfault = (value & (1 << 5));
	if(!(value & (1 << 12))){
		initialized = false; // Driver has an issue
	}
}

/**
 * Checks for a failed transfer and takes the semaphore for uart port
 */
bool MotorSimplemotion::prepareUartTransmit(){
	if(HAL_GetTick() - lastTimeByteReceived > uartErrorTimeout && uartErrorOccured){
//		uartport->abortReceive();
		resetBuffer();
	}
	return uartport->takeSemaphore(true,10);
}

/**
 * Sends a command buffer
 */
bool MotorSimplemotion::sendCommand(uint8_t* buf,uint8_t len,uint8_t adr){
	if(!prepareUartTransmit()){
		return false; // Failed
	}
	txbuf[0] = SMCMD_INSTANT_CMD;
	txbuf[1] = len;
	txbuf[2] = adr;
	memcpy(txbuf+3,buf,len);
	uint16_t crc = calculateCrc16_8_rev(tableCRC16,(uint8_t*)txbuf,len+3,0);
	txbuf[3+len] = (crc >> 8) & 0xff;
	txbuf[4+len] = (crc) & 0xff;

	if(uartport->transmit_IT((char*)(txbuf), len+5) && adr != 0) // Send update and wait for reply if not broadcasted
	{
		lastSentTime = HAL_GetTick();
		waitingReply = true;
		uartport->registerInterrupt(); // Listen for reply
		return true;
	}else{
		endUartTransfer(uartport, true); // Transfer aborted
		uartport->giveSemaphore(true);
		return false;
	}

}

/**
 * Appends a subpacket to a buffer.
 * Buffer must be 0 initialized
 * returns appended length
 */
uint8_t MotorSimplemotion::queueCommand(uint8_t* buf, MotorSimplemotion_cmdtypes type,uint32_t data){
	uint8_t len = 4 - ((uint8_t)type & 0x3);
	buf[0] = (((uint8_t)type) & 0x3) << (6); // First 2 bits
	data &= 0x3FFFFFFF; // Make sure first 2 bits are masked
	for(uint8_t b = 0;b<len;b++){
		buf[b] |= (data >> (8 * (len-1-b))) & 0xff; // copy data into buffer
	}
	return len;
}

/**
 * Resets the buffer and ends a transfer. must be called after every receive
 */
void MotorSimplemotion::resetBuffer(){
	memset((char*)rxbuf,0,RXBUF_SIZE);
	rxbuf_i = 0;
	waitingReply = false; // We failed if it was reset early
	waitingFastUpdate = false;
	uartErrorOccured = false;
	uartport->giveSemaphore(true); // When the buffer is reset we are allowed to transmit again
}

void MotorSimplemotion::uartRcv(char& buf){
	uint32_t errorcodes = uartport->getErrors();
	lastTimeByteReceived = HAL_GetTick();

	// Append to buffer while not overrun
	if(rxbuf_i < RXBUF_SIZE && !errorcodes){
		rxbuf[rxbuf_i++] = buf;
	}else{
		// Overrun
		uarterrors++;
		uartErrorOccured = true;
//		resetBuffer();
		/* We should actually NOT give back the semaphore immediately because data may still be sent by the device.
		 * Instead it should only be reset after a timeout...
		 */
		return;
	}
	// Check if we can parse a command
	char byte1 = rxbuf[0];

	if(waitingFastUpdate && byte1 == SMCMD_FAST_UPDATE_CYCLE_RET && rxbuf_i == 6) // Fast update reply
	{
		// We know the size of the fast update
		waitingFastUpdate = false;

		if(calculateCrc8(tableCRC8, (uint8_t*)rxbuf, 6, crc8init) != 0){
			uartErrorOccured = true;
			crcerrors++;
			resetBuffer();
			return;
		}
		Sm2FastUpdate_reply packet;
		memcpy(&packet,(char*)rxbuf,6);
		resetBuffer(); // Done
		updatePosition(packet.val1);
		updateStatus(packet.val2);

		return;
	}
	else if(waitingReply && byte1 == SMCMD_INSTANT_CMD_RET && rxbuf_i > 5) // Standard reply
	{

		uint8_t len = rxbuf[1];
		uint8_t adr = rxbuf[2];
		if(rxbuf_i < len+5){ // minimum 6 bytes, 3 header, 2 checksum + len * data
			uartport->registerInterrupt(); // Wait for next byte if nothing can be parsed yet
			return; // Not yet ready
		}
		if(adr != this->address){ // Intended for a different device
			resetBuffer();
			return;
		}

		// check that crc is 0
		if(calculateCrc16_8_rev(tableCRC16, (uint8_t*)rxbuf, rxbuf_i, 0)){
			crcerrors++;
			uartErrorOccured = true;
			resetBuffer();
			return;
		}
		// Frame should be valid from here on
		uint8_t i = 0;
		char* data = (char*)(rxbuf+3);
		replyidx = 0;
		while(i < len && replyidx < REPLYBUF_SIZE){
			uint8_t subpacketlen = 4 - ((data[i] >> 6) & 0x3); // MotorSimplemotion_cmdtypes
			uint32_t val = (data[i] & 0x3f) << ((subpacketlen-1)*8); // First byte contains upper 6 bits
			for(uint8_t b = 1;b < subpacketlen;b++){
				uint32_t data_t = (uint32_t)data[i+b];
				val |= data_t << ((subpacketlen-1-b)*8); // Copy next bytes in reverse
			}
			// Pad leading bits for negative values
//			val |= ~((1 << (subpacketlen*8))-1); // 8*bytes -1
			replyvalues[replyidx++] = val;
			i += subpacketlen;
		}
		waitingReply = false;
		resetBuffer();
		return;
	}
	else
	{
		uartport->registerInterrupt(); // Wait for next byte if nothing can be parsed yet
	}

}


void MotorSimplemotion::startMotor(){
	set1Parameter(MotorSimplemotion_param::CB1, 1);
}

void MotorSimplemotion::stopMotor(){
	turn(0);
	set1Parameter(MotorSimplemotion_param::CB1, 0);
}

EncoderType MotorSimplemotion::getEncoderType(){
		return EncoderType::absolute;
}

void MotorSimplemotion::startUartTransfer(UARTPort* port,bool transmit){
	// Semaphore must be taken before sending to lock temporary buffers
	if(transmit){
		writeEnablePin.set();
	}else{
		port->takeSemaphore(transmit);
	}
}
void MotorSimplemotion::endUartTransfer(UARTPort* port,bool transmit){
	// Disable write pin
	if(transmit){
		writeEnablePin.reset();
	}else{
		port->giveSemaphore(transmit); // Only give semaphore for receive action. We are only allowed to transmit again after the receive buffer is processed
	}

}

void MotorSimplemotion::registerCommands(){
	CommandHandler::registerCommands();

	registerCommand("crcerr", MotorSimplemotion_commands::crcerrors, "CRC error count",CMDFLAG_GET);
	registerCommand("uarterr", MotorSimplemotion_commands::uarterrors, "UART error count",CMDFLAG_GET);
	registerCommand("voltage", MotorSimplemotion_commands::voltage, "Voltage in mV",CMDFLAG_GET);
	registerCommand("torque", MotorSimplemotion_commands::torque, "Torque in mA",CMDFLAG_GET);
	registerCommand("state", MotorSimplemotion_commands::status, "Status flags",CMDFLAG_GET);
	registerCommand("restart", MotorSimplemotion_commands::restart, "Restart driver",CMDFLAG_GET);
	registerCommand("reg", MotorSimplemotion_commands::reg, "Read/Write raw register",CMDFLAG_GETADR | CMDFLAG_SETADR | CMDFLAG_DEBUG);
	registerCommand("devtype", MotorSimplemotion_commands::devtype, "Device type",CMDFLAG_GET);
}

CommandStatus MotorSimplemotion::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<MotorSimplemotion_commands>(cmd.cmdId)){
	case MotorSimplemotion_commands::crcerrors:
		replies.emplace_back(this->crcerrors);
		break;
	case MotorSimplemotion_commands::uarterrors:
		replies.emplace_back(this->uarterrors);
		break;
	case MotorSimplemotion_commands::voltage:
		replies.emplace_back(getVoltage());
		break;
	case MotorSimplemotion_commands::torque:
		replies.emplace_back(getTorque());
		break;
	case MotorSimplemotion_commands::status:
		replies.emplace_back(status);
		break;
	case MotorSimplemotion_commands::devtype:
		replies.emplace_back(devicetype);
		break;
	case MotorSimplemotion_commands::restart:
		restart();
		break;
	case MotorSimplemotion_commands::reg:{
		if(cmd.type == CMDtype::getat){
			uint32_t t=0;
			read1Parameter((MotorSimplemotion_param)cmd.adr, &t);
			replies.emplace_back((int32_t)t);
		}else if(cmd.type == CMDtype::setat){
			uint32_t t=0;
			set1Parameter((MotorSimplemotion_param)cmd.adr,cmd.val, &t);
			replies.emplace_back((int32_t)t);
		}
		break;
	}

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;

}

#endif
