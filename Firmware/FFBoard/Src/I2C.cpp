/*
 * I2C.cpp
 *
 *  Created on: 10.03.2022
 *      Author: Yannick
 */

#include <I2C.h>

I2CPort::I2CPort(I2C_HandleTypeDef &hi2c) : hi2c(hi2c) {

}

I2CPort::~I2CPort() {

}


bool I2CPort::configurePort(I2C_InitTypeDef* config){
	if(config == nullptr || hi2c.Init == *config){
		return; // No need to reconfigure
	}
	hi2c.Init = *config;
	return (HAL_I2C_Init(&hi2c) == HAL_OK);
}

bool I2CPort::transmitMaster(const uint16_t addr,const uint8_t* pData,const uint16_t size,const uint32_t timeout){
	return HAL_I2C_Master_Transmit(&this->hi2c, addr, pData, size, timeout) == HAL_OK;
}

bool I2CPort::receiveMaster(const uint16_t addr,uint8_t* pData,const uint16_t Size,const uint32_t timeout){
	return HAL_I2C_Master_Receive(&this->hi2c, addr, pData, size, timeout) == HAL_OK;
}

bool I2CPort::writeMem(const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,const uint8_t* pData,const uint16_t size,const uint32_t timeout){
	return HAL_I2C_Mem_Write(&this->hi2c, devAddr, memAddr, memAddSize, pData, size, timeout) == HAL_OK;
}
bool I2CPort::readMem(const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout){
	return HAL_I2C_Mem_Read(&this->hi2c, devAddr, memAddr, memAddSize, pData, size, timeout) == HAL_OK;
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
	isTakenFlag = true;
}

void I2CPort::giveSemaphore(){
	bool isIsr = inIsr();
	isTakenFlag = false;
	if(isIsr){
		BaseType_t taskWoken = 0;
		this->semaphore.GiveFromISR(&taskWoken);
		portYIELD_FROM_ISR(taskWoken);
	}else{
		this->semaphore.Give();
	}

}
