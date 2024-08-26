/*
 * I2C.cpp
 *
 *  Created on: 10.03.2022
 *      Author: Yannick
 */

#include <I2C.h>
#ifdef I2CBUS
ClassIdentifier I2CPort::info = {
	.name = "I2C port",
	.id = CLSID_I2CPORT,
	.visibility = ClassVisibility::visible};



//static bool operator==(const I2C_InitTypeDef& lhs, const I2C_InitTypeDef& rhs) {
//	return lhs.AddressingMode == rhs.AddressingMode
//			&& lhs.ClockSpeed == rhs.ClockSpeed
//			&& lhs.DualAddressMode == rhs.DualAddressMode
//			&& lhs.DutyCycle == rhs.DutyCycle
//			&& lhs.GeneralCallMode == rhs.GeneralCallMode
//			&& lhs.NoStretchMode == rhs.NoStretchMode
//			&& lhs.OwnAddress1 == rhs.OwnAddress1
//			&& lhs.OwnAddress2 == rhs.OwnAddress2;
//}

//static bool operator==(const I2C_InitTypeDef& lhs, const I2C_InitTypeDef& rhs) {
//	return memcmp(&lhs,&rhs,sizeof(I2C_InitTypeDef)) == 0;
//}
//



I2CPort::I2CPort(I2C_HandleTypeDef &hi2c,const I2CPortHardwareConfig& presets,uint8_t instance) : CommandHandler("i2c", CLSID_I2CPORT, instance), hi2c(hi2c),presets(presets) {
	restoreFlashDelayed();
#ifdef I2C_COMMANDS_DISABLED_IF_NOT_USED
	this->setCommandsEnabled(false);
#endif
	registerCommands();
}

I2CPort::~I2CPort() {

}

void I2CPort::saveFlash(){
	if(this->getCommandHandlerInfo()->instance != 0){
		return; // Only first instance can save
	}
	uint16_t data = (this->speedPreset & 0b11); // 2 bits reserved. 1 used
	Flash_Write(ADR_I2CCONF1, data);
}

void I2CPort::restoreFlash(){
	if(this->getCommandHandlerInfo()->instance != 0){
		return; // Only first instance can save
	}
	uint16_t data;
	if(Flash_Read(ADR_I2CCONF1, &data)){
		setSpeedPreset(data & 0b11);
	}
}

void I2CPort::setSpeedPreset(uint8_t preset){

	speedPreset = std::min<uint8_t>(preset,presets.presets.size());
	config = presets.getPreset(speedPreset).init;

	if(portUsers)
		configurePort(&config);
}

uint8_t I2CPort::getSpeedPreset(){
	return speedPreset;
}

void I2CPort::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("speed", I2CPort_commands::speed, "I2C speed preset", CMDFLAG_GET|CMDFLAG_SET|CMDFLAG_INFOSTRING);
}

CommandStatus I2CPort::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<I2CPort_commands>(cmd.cmdId)){

	case I2CPort_commands::speed:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->speedPreset);
		}else if(cmd.type == CMDtype::set && presets.canChangeSpeed){
			setSpeedPreset(cmd.val);
		}else if(cmd.type == CMDtype::info){
			for(uint8_t i = 0; i<presets.presets.size();i++){
				replies.emplace_back(std::string(presets.presets[i].name)  + ":" + std::to_string(i));
			}
		}
	break;

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}

/**
 * Forces a reinit
 */
void I2CPort::resetPort(){
	configurePort(&this->config);
	giveSemaphore();
}

/**
 * Signals that this port is being used.
 * Increments the user counter
 */
void I2CPort::takePort(I2CDevice* device){
	if(portUsers++ == 0){
		this->config = hi2c.Init;
		//configurePort(&this->config);
		setSpeedPreset(speedPreset); // Load speed and activate
#ifdef I2C_COMMANDS_DISABLED_IF_NOT_USED
		this->setCommandsEnabled(true);
#endif
	}
}

/**
 * Signals that the port is not needed anymore.
 * Decrements the user counter
 */
void I2CPort::freePort(I2CDevice* device){
	if(portUsers>0){
		portUsers--;
	}
	if(device == currentDevice){
		currentDevice = nullptr; // invalidate pointer if a transfer was in progress
		giveSemaphore();
	}
	if(portUsers == 0){

		HAL_I2C_DeInit(&hi2c);
#ifdef I2C_COMMANDS_DISABLED_IF_NOT_USED
		this->setCommandsEnabled(false);
#endif
	}

}

void I2CPort::configurePort(I2C_InitTypeDef* config){
	if(config == nullptr){
		return; // No need to reconfigure
	}
	this->config = *config;
	hi2c.Init = this->config;
	HAL_I2C_Init(&hi2c);
}

bool I2CPort::transmitMaster(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,const uint32_t timeout,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Master_Transmit(&this->hi2c, shiftAddr ? addr << 1 : addr, pData, size, timeout) == HAL_OK;
	device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::transmitMasterDMA(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	return HAL_I2C_Master_Transmit_DMA(&this->hi2c, shiftAddr ? addr << 1 : addr, pData, size) == HAL_OK;
}

bool I2CPort::transmitMasterIT(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	return HAL_I2C_Master_Transmit_IT(&this->hi2c, shiftAddr ? addr << 1 : addr, pData, size) == HAL_OK;
}

bool I2CPort::receiveMaster(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,const uint32_t timeout,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Master_Receive(&this->hi2c, shiftAddr ? addr << 1 : addr, pData, size, timeout) == HAL_OK;
	device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::receiveMasterDMA(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	return HAL_I2C_Master_Receive_DMA(&this->hi2c, shiftAddr ? addr << 1 : addr, pData, size) == HAL_OK;
}

bool I2CPort::receiveMasterIT(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	return HAL_I2C_Master_Receive_IT(&this->hi2c, shiftAddr ? addr << 1 : addr, pData, size) == HAL_OK;
}


bool I2CPort::writeMem(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Mem_Write(&this->hi2c, shiftAddr ? devAddr << 1 : devAddr, memAddr, memAddSize, pData, size, timeout) == HAL_OK;
	device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::readMem(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Mem_Read(&this->hi2c, shiftAddr ? devAddr << 1 : devAddr, memAddr, memAddSize, pData, size, timeout) == HAL_OK;
	device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::readMemIT(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Mem_Read_IT(&this->hi2c, shiftAddr ? devAddr << 1 : devAddr, memAddr, memAddSize, pData, size) == HAL_OK;
	//device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::writeMemIT(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Mem_Write_IT(&this->hi2c, shiftAddr ? devAddr << 1 : devAddr, memAddr, memAddSize, pData, size) == HAL_OK;
	//device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::isDeviceReady(I2CDevice* device,const uint16_t devAddr,const uint32_t trials,uint32_t timeout,bool shiftAddr){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_IsDeviceReady(&this->hi2c, shiftAddr ? devAddr << 1 : devAddr, trials,timeout) == HAL_OK;
	device->endI2CTransfer(this);
	return flag;
}

void I2CPort::I2cTxCplt(I2C_HandleTypeDef *hi2c){
	if (currentDevice == nullptr) {
		return;
	}

	if (hi2c->Instance != this->hi2c.Instance) {
		return;
	}
	currentDevice->endI2CTransfer(this);
	currentDevice->i2cTxCompleted(this);

	currentDevice = nullptr;
}

void I2CPort::I2cRxCplt(I2C_HandleTypeDef *hi2c){
	if (currentDevice == nullptr) {
		return;
	}

	if (hi2c->Instance != this->hi2c.Instance) {
		return;
	}
	currentDevice->endI2CTransfer(this);
	currentDevice->i2cRxCompleted(this);

	currentDevice = nullptr;
}

void I2CPort::I2cError(I2C_HandleTypeDef *hi2c){
	if (currentDevice){
		currentDevice->i2cError(this);
	}else{
		resetPort();
	}
}

bool I2CPort::isTaken(){
	return hi2c.State != HAL_I2C_STATE_READY;
}

void I2CPort::takeSemaphore(){
	bool isIsr = inIsr();

	if(isIsr){
		BaseType_t taskWoken = 0;
		this->semaphore.TakeFromISR(&taskWoken);
		portYIELD_FROM_ISR(taskWoken);
	}else{
		this->semaphore.Take();
	}
	//isTakenFlag = true;
}

void I2CPort::giveSemaphore(){
	bool isIsr = inIsr();
	//isTakenFlag = false;
	if(isIsr){
		BaseType_t taskWoken = 0;
		this->semaphore.GiveFromISR(&taskWoken);
		portYIELD_FROM_ISR(taskWoken);
	}else{
		this->semaphore.Give();
	}

}

I2CDevice::I2CDevice(){

}
I2CDevice::~I2CDevice(){

}

void I2CDevice::startI2CTransfer(I2CPort* port){
	port->takeSemaphore();
}

void I2CDevice::endI2CTransfer(I2CPort* port){
	port->giveSemaphore();
}


void I2CDevice::i2cTxCompleted(I2CPort* port){

}
void I2CDevice::i2cRxCompleted(I2CPort* port){

}
void I2CDevice::i2cError(I2CPort* port){
	port->resetPort();
}
#endif
