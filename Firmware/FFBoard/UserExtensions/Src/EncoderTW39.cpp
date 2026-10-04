/*
 * EncoderTW39.cpp
 *
 *  Created on: Oct 4, 2026
 *      Author: Yannick
 */

#include "EncoderTW39.h"
#include "constants.h"
#ifdef TW39ENCODER
bool EncoderTW39::inUse = false;
ClassIdentifier EncoderTW39::info = {
		 .name = "TW39" ,
		 .id=CLSID_ENCODER_TW39
};

const ClassIdentifier EncoderTW39::getInfo(){
	return info;
}


EncoderTW39::EncoderTW39() : SPIDevice(ENCODER_SPI_PORT,ENCODER_SPI_PORT.getFreeCsPins()[0]), CommandHandler("tw39enc",CLSID_ENCODER_TW39,0),cpp_freertos::Thread("TW39",256,42) {
	EncoderTW39::inUse = true;
	this->spiConfig.peripheral.BaudRatePrescaler = spiPort.getClosestPrescaler(10e6).first; // 20MHz max
	this->spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	this->spiConfig.peripheral.CLKPhase = SPI_PHASE_1EDGE;
	this->spiConfig.peripheral.CLKPolarity = SPI_POLARITY_LOW;
	this->spiConfig.peripheral.DataSize = SPI_DATASIZE_8BIT;
	this->spiConfig.cspol = true;

	restoreFlash(); // Also configures SPI port
	spiPort.reserveCsPin(this->spiConfig.cs);

	CommandHandler::registerCommands();
	registerCommand("cs", EncoderTW39_commands::cspin, "CS pin",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("speed", EncoderTW39_commands::speed, "SPI speed preset",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("pos", EncoderTW39_commands::pos, "Position",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("errors", EncoderTW39_commands::errors, "Error count",CMDFLAG_GET);
	registerCommand("chip", EncoderTW39_commands::chip, "Chip id:revision. 0 if not found",CMDFLAG_GET);
	registerCommand("serial", EncoderTW39_commands::serial, "Chip serial number",CMDFLAG_GET);
	registerCommand("status", EncoderTW39_commands::status, "Latched status:fatal status. Set to clear",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("temp", EncoderTW39_commands::temp, "Chip temperature",CMDFLAG_GET);
	registerCommand("outmode", EncoderTW39_commands::outmode, "Incremental output mode",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("ssi", EncoderTW39_commands::ssi, "Serial protocol (BiSS=0;SSI=1)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("abzres", EncoderTW39_commands::abzres, "ABZ edges per rotation (4x cycles)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("abzcfg", EncoderTW39_commands::abzcfg, "ABZ config (b0-1:zwidth,b2:dir,b3:apol,b4:bpol,b5:zpol)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("uvwpairs", EncoderTW39_commands::uvwpairs, "UVW pole pairs (0=32)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("calib", EncoderTW39_commands::calib, "Auto calibration (1=start;0=finish and store)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("save", EncoderTW39_commands::save, "Save config to encoder memory",CMDFLAG_GET);
	registerCommand("reg", EncoderTW39_commands::reg, "Read/Write register",CMDFLAG_GETADR | CMDFLAG_SETADR | CMDFLAG_DEBUG);
	registerCommand("cmd", EncoderTW39_commands::cmd, "Write command register",CMDFLAG_SET | CMDFLAG_DEBUG);
	this->Start();
}

EncoderTW39::~EncoderTW39() {
	EncoderTW39::inUse = false;
	spiPort.freeCsPin(this->spiConfig.cs);
}

void EncoderTW39::restoreFlash(){
	uint16_t conf_int = Flash_ReadDefault(ADR_ENCTW39_CONF1, 0);

	uint8_t cspin = conf_int & 0x3;
	offset = Flash_ReadDefault(ADR_ENCTW39_OFS, 0) << (posBits-16);
	setCsPin(cspin);
	setSpiSpeed((conf_int >> 2) & 0x3);
}

void EncoderTW39::saveFlash(){
	uint16_t conf_int = this->cspin & 0x3;
	conf_int |= (this->spiSpeedPreset & 0x3) << 2;
	Flash_Write(ADR_ENCTW39_CONF1, conf_int);
	Flash_Write(ADR_ENCTW39_OFS, offset >> (posBits-16));
}


void EncoderTW39::Run(){
	Delay(10); // Chip startup time
	readChipInfo();
	while(true){
		requestNewDataSem.Take(); // Wait until a position is requested
		transferMutex.Lock();
		updateAngleStatus();
		this->WaitForNotification();  // Wait until DMA is finished
		transferMutex.Unlock();

		if(updateAngleStatusCb()){
			int overflowLim = getCpr() >> 1;
			if(curAngleInt-lastAngleInt > overflowLim){ // Underflowed
				rotations--;
			}
			else if(lastAngleInt-curAngleInt > overflowLim){ // Overflowed
				rotations++;
			}
			lastAngleInt = curAngleInt;

			curPos = rotations * getCpr() + curAngleInt; // Update position
		}else{
			errors++;
		}
		lastUpdateTick = HAL_GetTick();
		waitForUpdateSem.Give();
		updateInProgress = false;
	}
}

void EncoderTW39::setCsPin(uint8_t cspin){
	spiPort.freeCsPin(this->spiConfig.cs);
	this->cspin = std::min<uint8_t>(spiPort.getCsPins().size()-1, cspin);
	this->spiConfig.cs = *spiPort.getCsPin(this->cspin);
	initSPI();
	spiPort.reserveCsPin(this->spiConfig.cs);
}

void EncoderTW39::initSPI(){
	spiPort.takeSemaphore();
	spiPort.configurePort(&this->spiConfig.peripheral);
	spiPort.giveSemaphore();
}

/**
 * Reads a 16 bit register.
 * The reply is received in the following packet which is a position read so that a position update can follow directly
 */
uint16_t EncoderTW39::readReg(uint16_t addr){
	uint8_t txbufReg[packetLen] = {0x30,0,(uint8_t)(addr >> 8),(uint8_t)(addr & 0xff),0,0,0,0}; // reg=1, rm=4
	uint8_t rxbufReg[packetLen] = {0};
	transferMutex.Lock();
	spiPort.transmitReceive(txbufReg, rxbufReg, packetLen, this,100);
	spiPort.transmitReceive(txbuf, rxbufReg, packetLen, this,100);
	transferMutex.Unlock();
	return (rxbufReg[0] << 8) | rxbufReg[1];
}

/**
 * Writes a 16 bit register
 */
void EncoderTW39::writeReg(uint16_t addr,uint16_t data){
	uint8_t txbufReg[packetLen] = {0x13,0,(uint8_t)(addr >> 8),(uint8_t)(addr & 0xff),(uint8_t)(data >> 8),(uint8_t)(data & 0xff),0,0}; // rm=4, wm=3
	uint8_t rxbufReg[packetLen] = {0};
	transferMutex.Lock();
	spiPort.transmitReceive(txbufReg, rxbufReg, packetLen, this,100);
	transferMutex.Unlock();
}

/**
 * Changes only the masked bits of a register
 */
void EncoderTW39::writeRegBits(EncoderTW39_reg reg,uint16_t mask,uint16_t data){
	uint16_t val = readReg(reg);
	val = (val & ~mask) | (data & mask);
	writeReg(reg,val);
}

void EncoderTW39::sendChipCommand(EncoderTW39_chipcmd cmd){
	writeReg(EncoderTW39_reg::COMMAND,(uint8_t)cmd);
}

/**
 * Returns the currently executing command or 0 if idle
 */
uint8_t EncoderTW39::getChipCommand(){
	return readReg(EncoderTW39_reg::COMMAND) & 0xff;
}

/**
 * Reads the chip id, revision and serial number
 * Returns true if a TW39 was found
 */
bool EncoderTW39::readChipInfo(){
	chipId = readReg(EncoderTW39_reg::CHIP_ID);
	connected = chipId == chipIdTW39;
	if(connected){
		chipRev = readReg(EncoderTW39_reg::CHIP_REV);
		serial = readReg(EncoderTW39_reg::SERIAL_L) | ((uint32_t)readReg(EncoderTW39_reg::SERIAL_H) << 16);
	}else{
		chipId = 0;
		chipRev = 0;
		serial = 0;
	}
	return connected;
}

/**
 * Starts or finishes the auto calibration. Same as the calibration button.
 * The magnet must be rotated multiple times while active.
 * When finished the encoder stores the calibration in its eeprom
 */
void EncoderTW39::setCalibration(bool enable){
	if(enable){
		keyBeforeCalib = readReg(EncoderTW39_reg::BISS_KEY);
		writeReg(EncoderTW39_reg::BISS_KEY,bissKeyUnlock);
		sendChipCommand(EncoderTW39_chipcmd::autocal);
	}else if(getChipCommand() == (uint8_t)EncoderTW39_chipcmd::autocal){
		sendChipCommand(EncoderTW39_chipcmd::idle);
		writeReg(EncoderTW39_reg::BISS_KEY,keyBeforeCalib);
	}
}

/**
 * Saves current configuration to permanent storage in the encoder
 * Blocks until done. The eeprom is only rated for 1000 writes.
 */
bool EncoderTW39::saveEeprom(){
	uint16_t key = readReg(EncoderTW39_reg::BISS_KEY);
	writeReg(EncoderTW39_reg::BISS_KEY,bissKeyUnlock);
	writeRegBits(EncoderTW39_reg::TEST,0x1,0x1); // Unlock eeprom
	sendChipCommand(EncoderTW39_chipcmd::writeEeprom);

	bool done = false;
	uint32_t startTick = HAL_GetTick();
	while(!done && HAL_GetTick() - startTick < eepromTimeout){
		vTaskDelay(pdMS_TO_TICKS(20));
		done = getChipCommand() == (uint8_t)EncoderTW39_chipcmd::idle;
	}

	writeRegBits(EncoderTW39_reg::TEST,0x1,0x0);
	writeReg(EncoderTW39_reg::BISS_KEY,key);
	return done;
}

void EncoderTW39::setPos(int32_t pos){
	offset = curPos - pos;
}


void EncoderTW39::spiTxRxCompleted(SPIPort* port){

	if(updateInProgress){
		memcpy(rxbuf,rxbuf_t,sizeof(rxbuf));
		NotifyFromISR();
	}
}

void EncoderTW39::spiRequestError(SPIPort* port){
	if(updateInProgress){
		transferError = true;
		NotifyFromISR();
	}
}


/**
 * Requests a position packet. The reply contains the position sampled at the start of this transfer
 */
void EncoderTW39::updateAngleStatus(){
	transferError = false;
	spiPort.transmitReceive_DMA(txbuf, rxbuf_t, packetLen, this);
}

bool EncoderTW39::updateAngleStatusCb(){
	// Bytes 0-3 are the revolution count, 4-7 the angle and 6 status bits
	uint32_t angleStatus = ((uint32_t)rxbuf[4] << 24) | ((uint32_t)rxbuf[5] << 16) | ((uint32_t)rxbuf[6] << 8) | rxbuf[7];
	spiStatus = angleStatus & 0x3f;
	bool fatal = spiStatus & 0x20; // fflt. Position invalid

	if(transferError || fatal){
		return false;
	}
	curAngleInt = (angleStatus >> 6) >> (angleBits - posBits);
	return true;
}

int32_t EncoderTW39::getPos(){

	return getPosAbs() - offset;
}

int32_t EncoderTW39::getPosAbs(){
	if(updateInProgress){ // If a transfer is still in progress return the last result
		return curPos;
	}
	updateInProgress = true;
	requestNewDataSem.Give(); // Start transfer
	if(HAL_GetTick() - lastUpdateTick > waitThresh)
		waitForUpdateSem.Take(waitThresh); // Wait a bit

	return curPos;
}

uint32_t EncoderTW39::getCpr(){
	return 1 << posBits;
}

void EncoderTW39::setSpiSpeed(uint8_t preset){
	if(preset == spiSpeedPreset){
		return; // Ignore if no change
	}
	spiSpeedPreset = clip<uint8_t,uint8_t>(preset,0,spispeeds.size()-1);
	this->spiConfig.peripheral.BaudRatePrescaler = spiPort.getClosestPrescaler(spispeeds[spiSpeedPreset]).first;
	initSPI();
}


CommandStatus EncoderTW39::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<EncoderTW39_commands>(cmd.cmdId)){
	case EncoderTW39_commands::cspin:
		if(cmd.type==CMDtype::get){
			replies.emplace_back(this->cspin+1);
		}else if(cmd.type==CMDtype::set){
			this->setCsPin(cmd.val-1);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::speed:
	{
		if(cmd.type == CMDtype::get){
			replies.emplace_back(spiSpeedPreset);
		}else if(cmd.type == CMDtype::set){
			setSpiSpeed(cmd.val);
		}else if(cmd.type == CMDtype::info){
			for(uint8_t i = 0; i<spispeeds.size();i++){
				replies.emplace_back(std::to_string(this->spiPort.getClosestPrescaler(spispeeds[i]).second)  + ":" + std::to_string(i)+"\n");
			}
		}else{
			return CommandStatus::ERR;
		}
		break;
	}

	case EncoderTW39_commands::pos:
		if(cmd.type==CMDtype::get){
			replies.emplace_back(getPos());
		}else if(cmd.type==CMDtype::set){
			this->setPos(cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::errors:
		replies.emplace_back(errors);
		break;

	case EncoderTW39_commands::chip:
		readChipInfo(); // Check again. CS pin might have changed
		replies.emplace_back(chipId,chipRev);
		break;

	case EncoderTW39_commands::serial:
		replies.emplace_back(serial);
		break;

	case EncoderTW39_commands::status:
		if(cmd.type==CMDtype::get){
			replies.emplace_back(readReg(EncoderTW39_reg::STAT_LATCH),readReg(EncoderTW39_reg::STAT_FATAL));
		}else if(cmd.type==CMDtype::set){
			sendChipCommand(EncoderTW39_chipcmd::clearStatus);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::temp:
		replies.emplace_back((int16_t)readReg(EncoderTW39_reg::T_NOW));
		break;

	case EncoderTW39_commands::outmode:
		if(cmd.type==CMDtype::get){
			replies.emplace_back((readReg(EncoderTW39_reg::MAIN_CFG) >> 7) & 0x7);
		}else if(cmd.type==CMDtype::set){
			writeRegBits(EncoderTW39_reg::MAIN_CFG, 0x7 << 7, cmd.val << 7);
			sendChipCommand(EncoderTW39_chipcmd::restart);
		}else if(cmd.type==CMDtype::info){
			replies.emplace_back("None:0\nABZ:1\nUVW:2");
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::ssi:
		if(cmd.type==CMDtype::get){
			replies.emplace_back((readReg(EncoderTW39_reg::BISS_CFG0) >> 8) & 0x1);
		}else if(cmd.type==CMDtype::set){
			writeRegBits(EncoderTW39_reg::BISS_CFG0, 0x1 << 8, (cmd.val ? 1 : 0) << 8);
			sendChipCommand(EncoderTW39_chipcmd::restart);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::abzres:
		if(cmd.type==CMDtype::get){
			replies.emplace_back(readReg(EncoderTW39_reg::ABZ_RES_L) | ((readReg(EncoderTW39_reg::ABZ_RES_H) & 0xf) << 16));
		}else if(cmd.type==CMDtype::set){
			uint32_t res = clip<int64_t,int64_t>(cmd.val,4,262144);
			writeReg(EncoderTW39_reg::ABZ_RES_L, res & 0xffff);
			writeReg(EncoderTW39_reg::ABZ_RES_H, (res >> 16) & 0xf);
			sendChipCommand(EncoderTW39_chipcmd::restart);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::abzcfg:
		if(cmd.type==CMDtype::get){
			replies.emplace_back((readReg(EncoderTW39_reg::ABZ_CFG) >> 5) & 0x3f);
		}else if(cmd.type==CMDtype::set){
			writeRegBits(EncoderTW39_reg::ABZ_CFG, 0x3f << 5, cmd.val << 5);
			sendChipCommand(EncoderTW39_chipcmd::restart);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::uvwpairs:
		if(cmd.type==CMDtype::get){
			replies.emplace_back(readReg(EncoderTW39_reg::UVW_CFG) & 0x1f);
		}else if(cmd.type==CMDtype::set){
			writeRegBits(EncoderTW39_reg::UVW_CFG, 0x1f, cmd.val);
			sendChipCommand(EncoderTW39_chipcmd::restart);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::calib:
		if(cmd.type==CMDtype::get){
			replies.emplace_back(getChipCommand() == (uint8_t)EncoderTW39_chipcmd::autocal ? 1 : 0);
		}else if(cmd.type==CMDtype::set){
			setCalibration(cmd.val != 0);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case EncoderTW39_commands::save:
	{
		if(cmd.type == CMDtype::get){
			replies.emplace_back(saveEeprom() ? 1 : 0);
		}
		break;
	}

	case EncoderTW39_commands::reg:
	{
		if(cmd.type == CMDtype::getat){
			replies.emplace_back(readReg((uint16_t)cmd.adr));
		}else if(cmd.type == CMDtype::setat){
			writeReg((uint16_t)cmd.adr, cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;
	}

	case EncoderTW39_commands::cmd:
		if(cmd.type == CMDtype::set){
			writeReg(EncoderTW39_reg::COMMAND, cmd.val & 0xff);
		}else{
			return CommandStatus::ERR;
		}
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
}

#endif
