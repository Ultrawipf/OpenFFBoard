/*
 * FFBWheel.h
 *
 *  Created on: 29.03.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_FFBWHEEL_H_
#define USEREXTENSIONS_SRC_FFBWHEEL_H_

#include "FFBHIDMain.h"

class FFBWheel : public FFBHIDMain {
public:
	FFBWheel();
	virtual ~FFBWheel();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void usbInit() override;
};

#endif /* USEREXTENSIONS_SRC_FFBWHEEL_H_ */
