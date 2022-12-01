/*
 * FFBWheel.h
 *
 *  Created on: 29.03.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_FFBWHEEL_H_
#define USEREXTENSIONS_SRC_FFBWHEEL_H_
#include "constants.h"
#ifdef FFBWHEEL

#include "FFBHIDMain.h"

class FFBWheel : public FFBHIDMain {
public:
	FFBWheel();
	virtual ~FFBWheel();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void usbInit() override;


private:
	std::unique_ptr<EffectsCalculator> effects_calc = std::make_unique<EffectsCalculator>();
	std::unique_ptr<EffectsControlItf> ffb = std::make_unique<HidFFB>(*effects_calc,1);
};

#endif

#endif /* USEREXTENSIONS_SRC_FFBWHEEL_H_ */
