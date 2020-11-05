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
//#include "CommandHandler.h"



/*
 * All button sources have the ability to parse commands.
 * If a command is supported set "commandsEnabled(true)" and implement command function from CommandHandler
 */
class ButtonSource : virtual ChoosableClass,public PersistentStorage {
public:
	ButtonSource();
	virtual ~ButtonSource();

	virtual void readButtons(uint32_t* buf) = 0; // Return a bit field without offset of pressed buttons
	virtual uint16_t getBtnNum(); // Amount of readable buttons

	const virtual ClassIdentifier getInfo() = 0;
	static ClassIdentifier info;

protected:
	uint16_t btnnum = 0; // Amount of active buttons (valid bitfield length) to report
};

#endif /* BUTTONSOURCE_H_ */
