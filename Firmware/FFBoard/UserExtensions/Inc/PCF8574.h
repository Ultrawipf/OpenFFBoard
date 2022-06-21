/*
 * PCF8574.h
 *
 *  Created on: 10.03.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_PCF8574_H_
#define USEREXTENSIONS_SRC_PCF8574_H_



#include "ButtonSource.h"
#include "CommandHandler.h"
#include "cppmain.h"
#include "ChoosableClass.h"
#include "I2C.h"
#include "cpp_target_config.h"
#include "PersistentStorage.h"
#include "thread.hpp"
#ifdef I2C_PORT
class PCF8574 : public I2CDevice {
public:
	PCF8574(I2CPort &port);
	virtual ~PCF8574();

	//void configurePort(bool fastMode);

	uint8_t readByte(const uint8_t devAddr);
	void readByteIT(const uint8_t devAddr,uint8_t* data);
	void writeByteIT(const uint8_t devAddr,uint8_t* data);
	void writeByte(const uint8_t devAddr,uint8_t data);

//	void startI2CTransfer(I2CPort* port);
//	void endI2CTransfer(I2CPort* port);

//	void startI2CTransfer(I2CPort* port);
//	void endI2CTransfer(I2CPort* port);


protected:
	I2CPort &port;
	//I2C_InitTypeDef config;
	//volatile bool transferActive = false;
private:
	uint8_t lastWriteData = 0;
};

#ifdef PCF8574BUTTONS
class PCF8574Buttons : public PCF8574, public CommandHandler,public ButtonSource, public cpp_freertos::Thread {
public:
	PCF8574Buttons();
	virtual ~PCF8574Buttons();

	enum class PCF8574Buttons_commands : uint32_t {
		btnnum,invert,speed
	};
	void Run();

	void saveFlash();
	void restoreFlash();

	void i2cRxCompleted(I2CPort* port);
	void i2cError(I2CPort* port);

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;

	uint8_t readButtons(uint64_t* buf);
	uint16_t getBtnNum(); // Amount of readable buttons
	void setBtnNum(uint8_t num);

	const virtual ClassIdentifier getInfo();
	static ClassIdentifier info;
	static bool isCreatable() {return true;};

	const ClassType getClassType() override {return ClassType::Buttonsource;};

	std::string getHelpstring(){return "btnnum/8 devices required. Addresses starting at 0x20.";}

	void rxDone(uint8_t dat);

private:
	bool invert = true;
	uint8_t numBytes = 1;
	uint64_t mask = 0xff;
	uint64_t lastButtons = 0;
	uint64_t currentButtons = 0;

	uint8_t lastByte=0;
	uint8_t lastData=0;

	uint32_t lastSuccess=0;
	const uint32_t timeout = 250;
	volatile bool readingData = false;

};
#endif
#endif // I2C
#endif /* USEREXTENSIONS_SRC_PCF8574_H_ */
