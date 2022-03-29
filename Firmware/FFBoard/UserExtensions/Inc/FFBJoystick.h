/*
 * FFBJoystick.h
 *
 *  Created on: 29.03.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_FFBJOYSTICK_H_
#define USEREXTENSIONS_SRC_FFBJOYSTICK_H_

#include "FFBHIDMain.h"

class FFBJoystick : public FFBHIDMain {
public:
	FFBJoystick();
	virtual ~FFBJoystick();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void usbInit() override;
};

#endif /* USEREXTENSIONS_SRC_FFBJOYSTICK_H_ */
