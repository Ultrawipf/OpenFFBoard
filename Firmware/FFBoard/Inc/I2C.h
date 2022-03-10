/*
 * I2C.h
 *
 *  Created on: 10.03.2022
 *      Author: Yannick
 */

#ifndef SRC_I2C_H_
#define SRC_I2C_H_

#include "semaphore.hpp"
#include "cppmain.h"

//class I2CDevice;
class I2CPort {
public:
	I2CPort(I2C_HandleTypeDef &hi2c);
	virtual ~I2CPort();


	void configurePort(I2C_InitTypeDef* config); // Reconfigures the i2c port

	bool transmitMaster(const uint16_t addr,uint8_t* pData,const uint16_t size,const uint32_t timeout);
	bool receiveMaster(const uint16_t addr,uint8_t* pData,const uint16_t size,const uint32_t timeout);
	bool writeMem(const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout);
	bool readMem(const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout);

	void takeSemaphore();
	void giveSemaphore();
	bool isTaken();
private:
	cpp_freertos::BinarySemaphore semaphore = cpp_freertos::BinarySemaphore(true);
	volatile bool isTakenFlag = false;
	I2C_HandleTypeDef& hi2c;
};


//class I2CDevice{
//public:
//	I2CDevice();
//	I2CDevice(I2CPort& port);
//	virtual ~I2CDevice();
//
//	virtual void startI2CTransfer(I2CPort* port);
//	virtual void endI2CTransfer(I2CPort* port);
//
//protected:
//	I2CDevice* port = nullptr;
//};

#endif /* SRC_I2C_H_ */
