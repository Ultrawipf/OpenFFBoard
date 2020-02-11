/*
 * ButtonSource.h
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#ifndef BUTTONSOURCE_H_
#define BUTTONSOURCE_H_

#include "cppmain.h"
class ButtonSource {
public:
	ButtonSource();
	virtual ~ButtonSource();

	virtual void readButtons(uint8_t* buf ,uint16_t len) = 0;
	virtual uint16_t getBtnNum() = 0; // Amount of readable buttons
};

#endif /* BUTTONSOURCE_H_ */
