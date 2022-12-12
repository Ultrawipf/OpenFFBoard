/*
 * FFBHIDExt.h
 *
 *  Created on: 30.11.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_FFHIDEXT_H_
#define USEREXTENSIONS_SRC_FFHIDEXT_H_
#include "constants.h"
#ifdef FFBHIDEXT

#include "FFBHIDMain.h"
#include "SerialFFB.h"

class FFBHIDExt : public FFBHIDMain {
public:
	FFBHIDExt();
	virtual ~FFBHIDExt();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void usbInit() override;


private:
	std::shared_ptr<EffectsCalculator> effects_calc = std::make_shared<EffectsCalculator>();
	std::shared_ptr<EffectsControlItf> ffb = std::make_shared<SerialFFB>(effects_calc,0);
	std::shared_ptr<EffectsControlItf> ffb_axis2 = std::make_shared<SerialFFB>(effects_calc,1);
};

#endif

#endif /* USEREXTENSIONS_SRC_FFHIDEXT_H_ */
