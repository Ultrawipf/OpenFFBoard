/*
 * ADS111X.h
 *
 *  Created on: 21.04.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_ADS111X_H_
#define USEREXTENSIONS_SRC_ADS111X_H_

#include "I2C.h"
#include "CommandHandler.h"
#include "AnalogSource.h"
#include "PersistentStorage.h"
#include "thread.hpp"

class ADS111X : public I2CDevice {
public:
	ADS111X(I2CPort &port,uint8_t address = 0b1001000);
	virtual ~ADS111X();

//	void readRegIT(const uint8_t devAddr,const uint8_t reg,uint8_t* data);
//	void writeRegIT(const uint8_t devAddr,const uint8_t reg,uint16_t data);
//	void writeReg(const uint8_t devAddr,const uint8_t reg,uint16_t data);
//	void readReg(const uint8_t devAddr,const uint8_t reg,uint16_t data);
	void readRegIT(const uint8_t reg,uint8_t* data,uint8_t regsize);
//	void startI2CTransfer(I2CPort* port);
//	void endI2CTransfer(I2CPort* port);

	void startI2CTransfer(I2CPort* port);
	void endI2CTransfer(I2CPort* port);


protected:
	I2CPort &port;
	//I2C_InitTypeDef config;

	uint8_t address;

};


class ADS111X_AnalogSource : public ADS111X, public AnalogSource, public CommandHandler, public cpp_freertos::Thread {
	enum class ADS111X_AnalogSource_commands : uint32_t {
		axes,gain
	};

public:
	ADS111X_AnalogSource();
	~ADS111X_AnalogSource();

	void saveFlash();
	void restoreFlash();

	void Run();

	void i2cRxCompleted(I2CPort* port);

	std::vector<int32_t>* getAxes();

	const virtual ClassIdentifier getInfo();
	static ClassIdentifier info;
	static bool isCreatable() {return true;};
	std::string getHelpstring(){return "ADS1113/4/5 analog source";}

	const ClassType getClassType() override {return ClassType::Analogsource;};
private:
	bool readingData = false;
	uint16_t sampleBuffer;
};


#endif /* USEREXTENSIONS_SRC_ADS111X_H_ */
