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

class PCF8574 {
public:
	PCF8574(I2CPort &port);
	virtual ~PCF8574();

	uint8_t readByte(const uint8_t devAddr);
	void writeByte(const uint8_t devAddr,uint8_t data);

protected:
	I2CPort &port;
	I2C_InitTypeDef config;
private:
	uint8_t lastWriteData = 0;
};

/**
 *
 */
class PCF8574Buttons : public PCF8574, public CommandHandler,public ButtonSource{
public:
	PCF8574Buttons();
	virtual ~PCF8574Buttons();

	enum class PCF8574Buttons_commands : uint32_t {
		btnnum,invert
	};

	void saveFlash();
	void restoreFlash();

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;

	uint8_t readButtons(uint64_t* buf);
	uint16_t getBtnNum(); // Amount of readable buttons
	void setBtnNum(uint8_t num);

	const virtual ClassIdentifier getInfo();
	static ClassIdentifier info;
	static bool isCreatable() {return true;};

	const ClassType getClassType() override {return ClassType::Buttonsource;};

	std::string getHelpstring(){return "btnnum/8 devices required. Addresses starting at 0x20.";}

private:
	bool invert = true;
	uint8_t numBytes = 1;
	uint64_t mask = 0xff;
	uint64_t lastButtons = 0;
};

#endif /* USEREXTENSIONS_SRC_PCF8574_H_ */
