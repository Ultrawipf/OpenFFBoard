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
#include "vector"

/**
 * A button source can return up to 64 buttons
 */
class ButtonSource : public ChoosableClass,public PersistentStorage {
public:
	ButtonSource();
	virtual ~ButtonSource();
	/**
	 * Return a bit field without offset of pressed buttons in the supplied buffer.
	 * Returns amount of button read.
	 */
	virtual uint8_t readButtons(uint64_t* buf) = 0;
	virtual uint16_t getBtnNum(); // Amount of readable buttons

	const virtual ClassIdentifier getInfo() = 0;
	static ClassIdentifier info;
	static bool isCreatable() {return true;};
	virtual const ClassType getClassType() {return ClassType::Buttonsource;};

	static const std::vector<class_entry<ButtonSource> > all_buttonsources;

protected:
	uint16_t btnnum = 0; // Amount of active buttons (valid bitfield length) to report
};

#endif /* BUTTONSOURCE_H_ */
