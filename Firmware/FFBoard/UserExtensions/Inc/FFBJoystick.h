/*
 * FFBJoystick.h
 *
 *  Created on: 29.03.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_FFBJOYSTICK_H_
#define USEREXTENSIONS_SRC_FFBJOYSTICK_H_
#include "constants.h"
#ifdef FFBJOYSTICK

#include "FFBHIDMain.h"

class FFBJoystick : public FFBHIDMain {
public:
	FFBJoystick();
	virtual ~FFBJoystick();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void usbInit() override;

private:
	std::unique_ptr<EffectsCalculator> effects_calc = std::make_unique<EffectsCalculator>();
	std::unique_ptr<EffectsControlItf> ffb = std::make_unique<HidFFB>(*effects_calc,2);
};
#endif
#endif /* USEREXTENSIONS_SRC_FFBJOYSTICK_H_ */
