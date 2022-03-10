/*
 * I2C.cpp
 *
 *  Created on: 10.03.2022
 *      Author: Yannick
 */

#include <I2C.h>

static bool operator==(const I2C_InitTypeDef& lhs, const I2C_InitTypeDef& rhs) {
	return lhs.AddressingMode == rhs.AddressingMode
			&& lhs.ClockSpeed == rhs.ClockSpeed
			&& lhs.DualAddressMode == rhs.DualAddressMode
			&& lhs.DutyCycle == rhs.DutyCycle
			&& lhs.GeneralCallMode == rhs.GeneralCallMode
			&& lhs.NoStretchMode == rhs.NoStretchMode
			&& lhs.OwnAddress1 == rhs.OwnAddress1
			&& lhs.OwnAddress2 == rhs.OwnAddress2;
}




I2CPort::I2CPort(I2C_HandleTypeDef &hi2c) : hi2c(hi2c) {

}

I2CPort::~I2CPort() {

}

/**
 * Forces a reinit
 */
void I2CPort::resetPort(){
	HAL_I2C_Init(&hi2c);
}

void I2CPort::configurePort(I2C_InitTypeDef* config){
	if(config == nullptr || hi2c.Init == *config){
		return; // No need to reconfigure
	}
	hi2c.Init = *config;
	HAL_I2C_Init(&hi2c);
}

bool I2CPort::transmitMaster(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,const uint32_t timeout){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Master_Transmit(&this->hi2c, addr, pData, size, timeout) == HAL_OK;
	device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::transmitMasterDMA(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size){
	currentDevice = device;
	device->startI2CTransfer(this);
	return HAL_I2C_Master_Transmit_DMA(&this->hi2c, addr, pData, size) == HAL_OK;
}

bool I2CPort::transmitMasterIT(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size){
	currentDevice = device;
	device->startI2CTransfer(this);
	return HAL_I2C_Master_Transmit_IT(&this->hi2c, addr, pData, size) == HAL_OK;
}

bool I2CPort::receiveMaster(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,const uint32_t timeout){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Master_Receive(&this->hi2c, addr, pData, size, timeout) == HAL_OK;
	device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::receiveMasterDMA(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size){
	currentDevice = device;
	device->startI2CTransfer(this);
	return HAL_I2C_Master_Receive_DMA(&this->hi2c, addr, pData, size) == HAL_OK;
}

bool I2CPort::receiveMasterIT(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size){
	currentDevice = device;
	device->startI2CTransfer(this);
	return HAL_I2C_Master_Receive_IT(&this->hi2c, addr, pData, size) == HAL_OK;
}


bool I2CPort::writeMem(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Mem_Write(&this->hi2c, devAddr, memAddr, memAddSize, pData, size, timeout) == HAL_OK;
	device->endI2CTransfer(this);
	return flag;
}

bool I2CPort::readMem(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout){
	currentDevice = device;
	device->startI2CTransfer(this);
	bool flag = HAL_I2C_Mem_Read(&this->hi2c, devAddr, memAddr, memAddSize, pData, size, timeout) == HAL_OK;
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
