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
	std::unique_ptr<EffectsCalculator> effects_calc = std::make_unique<EffectsCalculator>();
	std::unique_ptr<EffectsControlItf> ffb = std::make_unique<SerialFFB>(*effects_calc);
};

#endif

#endif /* USEREXTENSIONS_SRC_FFHIDEXT_H_ */
