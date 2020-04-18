/*
 * ButtonSource.h
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#ifndef BUTTONSOURCE_H_
#define BUTTONSOURCE_H_

#include "cppmain.h"
#include "ChoosableClass.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"

struct ButtonSourceConfig{
	uint8_t numButtons = 32;
	bool cutRight = false; // if num buttons to read are not byte aligned specify where to shift
	bool invert = false;
	uint8_t extraOptions = 0; // Save anything extra here like special flags
};


/*
 * All button sources have the ability to parse commands.
 * If a command is supported set "commandsEnabled(true)" and implement command function from CommandHandler
 */
class ButtonSource : public ChoosableClass, public PersistentStorage,public CommandHandler {
public:
	ButtonSource();
	virtual ~ButtonSource();

	static ButtonSourceConfig decodeIntToConf(uint16_t val);
	static uint16_t encodeConfToInt(ButtonSourceConfig* c);

	virtual void readButtons(uint32_t* buf) = 0;
	virtual uint16_t getBtnNum(); // Amount of readable buttons
	virtual bool command(ParsedCommand* cmd,std::string* reply);

	void setConfig(ButtonSourceConfig config);
	virtual ButtonSourceConfig* getConfig();

	const uint16_t maxButtons = 32;

	const virtual ClassIdentifier getInfo() = 0;
	static ClassIdentifier info;


protected:
	virtual void process(uint32_t* buf);
	ButtonSourceConfig conf;
	uint32_t mask = 0xffffffff;
	uint16_t offset = 0;
	uint16_t bytes = 4;
};

#endif /* BUTTONSOURCE_H_ */
