/*
 * FFBShifter.h
 *
 *  Created on: 02.03.2023
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_FFBSHIFTER_H_
#define USEREXTENSIONS_SRC_FFBSHIFTER_H_
#include "constants.h"
#include <FFBoardMain.h>
#include "ButtonSource.h"
#include "Axis.h"
#include "thread.hpp"
#include "FFBHIDMain.h"
#include "EffectsCalculator.h"

#ifdef FFBSHIFTER

class FFBShifterEffects : public EffectsCalculatorItf , public EffectsControlItf{
private:
	bool active = false;
public:
	FFBShifterEffects();
	~FFBShifterEffects();
	bool isActive(){return this->active;}
	void setActive(bool active);
	void calculateEffects(std::vector<std::unique_ptr<Axis>> &axes);


	// Effects ITF
	void reset_ffb(){}; // Called when usb disconnected
};

class FFBShifter : public FFBHIDMain{

public:
	FFBShifter();
	virtual ~FFBShifter();


//	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
//	void registerCommands();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void usbInit() override;
	virtual std::string getHelpstring(){
		return "FFB HID Shifter";
	}

private:
	std::shared_ptr<FFBShifterEffects> effects_calc = std::make_shared<FFBShifterEffects>();

};
#endif
#endif /* USEREXTENSIONS_SRC_FFBSHIFTER_H_ */
