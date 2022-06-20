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
#include "cpp_target_config.h"
#include "target_constants.h"

#ifdef I2C_PORT

class ADS111X : public I2CDevice {

public:
	ADS111X(I2CPort &port,uint8_t address = 0x48);
	virtual ~ADS111X();

//	void readRegIT(const uint8_t devAddr,const uint8_t reg,uint8_t* data);
//	void writeRegIT(const uint8_t devAddr,const uint8_t reg,uint16_t data);
//	void writeReg(const uint8_t devAddr,const uint8_t reg,uint16_t data);
//	void readReg(const uint8_t devAddr,const uint8_t reg,uint16_t data);
	void readRegIT(const uint8_t reg,uint16_t* data);
	void writeRegIT(const uint8_t reg,uint16_t data);
	uint16_t readReg(const uint8_t reg);

//	void startI2CTransfer(I2CPort* port);
//	void endI2CTransfer(I2CPort* port);
	void startConversion(uint8_t channel, bool differential=false);

	void setGain(uint16_t gain);
	uint16_t getGain();

	void setDatarate(uint16_t rate);
	uint16_t getDatarate();

	void startI2CTransfer(I2CPort* port);
	void endI2CTransfer(I2CPort* port);


protected:
	I2CPort &port;
	//I2C_InitTypeDef config;

	uint8_t address;
	uint8_t datarate = 8;
	uint16_t conversions[4] = {0};

	struct {
		uint16_t config = 0x8583; // 0x01
		uint16_t lothresh = 0x8000; // 0x02
		uint16_t hithresh = 0x7FFF; // 0x03
	} registers;
private:
	uint16_t writeItBuffer;
};

#ifdef ADS111XANALOG
class ADS111X_AnalogSource : public ADS111X, public AnalogSource, public CommandHandler, public cpp_freertos::Thread {
	enum class ADS111X_AnalogSource_commands : uint32_t {
		axes,gain,address
	};

	enum class ADS111X_AnalogSource_state : uint8_t {
		none,idle,sampling,reading,getsample
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

	void initialize();
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);

private:
	//bool readingData = false;
	uint16_t sampleBuffer;
	uint8_t axes=4;
	uint8_t lastAxis = 0;
	uint32_t lastSuccess = 0;
	ADS111X_AnalogSource_state state = ADS111X_AnalogSource_state::none;
};
#endif
#endif /* USEREXTENSIONS_SRC_ADS111X_H_ */
#endif
