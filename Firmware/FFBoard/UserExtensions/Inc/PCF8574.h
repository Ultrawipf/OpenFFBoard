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

class PCF8574 {
public:
	PCF8574();
	virtual ~PCF8574();
};

/**
 *
 */
class PCF8574Buttons : public PCF8574, public ButtonSource,public CommandHandler {
public:
	PCF8574Buttons();
	virtual ~PCF8574Buttons();


	uint8_t readButtons(uint64_t* buf);
	uint16_t getBtnNum(); // Amount of readable buttons

	const virtual ClassIdentifier getInfo();
	static ClassIdentifier info;
	static bool isCreatable() {return true;};
};

#endif /* USEREXTENSIONS_SRC_PCF8574_H_ */
