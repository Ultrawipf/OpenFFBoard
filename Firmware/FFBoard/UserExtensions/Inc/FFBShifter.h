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
#include "CommandHandler.h"

#ifdef FFBSHIFTER

class FFBShifterEffects : public EffectsCalculatorItf , public EffectsControlItf, public CommandHandler, public ButtonSource{
	enum class FFBShifterEffectMode : uint8_t {sequential,h_sym};
	enum class FFBShifterEffect_commands {active,invert,mode};
	struct ShifterEffectParams{
		float gainYseq = 2;
		float gainY = 0.5;
		float gainX = 0.5;
		float gainXgate = 5;
		float seqgainX = 5;
		float seqsnap = 3;
		float seqsnappoint = 0.499;
		uint16_t maxForceY = 20000;
		uint16_t maxForceX= 20000;
		uint16_t maxCenterForceX= 5000;
		uint8_t hgatesX = 3;

		float range = 1; // Scales total movement range
		float rangeX = 1; // Scales total movement range
	};
private:
	bool active = true;
	bool invertAxes = true;
	FFBShifterEffectMode mode = FFBShifterEffectMode::h_sym;
	uint64_t buttons = 0;

public:
	FFBShifterEffects();
	~FFBShifterEffects();
	bool isActive(){return this->active;}
	void setActive(bool active);
	void calculateEffects(std::vector<std::unique_ptr<Axis>> &axes);
	void calculateShifterEffect(metric_t* metricsX,metric_t* metricsY, int32_t* torqueX,int32_t* torqueY);
//	int32_t calculateShifterEffectX( metric_t* metrics);
//	int32_t calculateShifterEffectY( metric_t* metrics);

	uint8_t readButtons(uint64_t* buf);

	void setMode(FFBShifterEffectMode mode);
	void setMode_i(const uint8_t mode);

	// Effects ITF
	void reset_ffb(){}; // Called when usb disconnected

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);

	std::string getHelpstring(){return "2 Axis FFB shifter effects";};
	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	int32_t springEffect(int32_t position,int32_t offset,float coefficient,int32_t negativeSaturation=0x7fff,int32_t positiveSaturation=0x7fff,int32_t deadBand=0);

	ShifterEffectParams params;
};

class FFBShifter : public FFBHIDMain{

public:
	FFBShifter();
	virtual ~FFBShifter();


	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void usbInit() override;
	virtual std::string getHelpstring(){
		return "FFB HID Shifter";
	}

	static const std::array<char*,2> modenames;

//	uint8_t updateAnalog() override;
//	uint8_t updateButtons(uint8_t initialShift = 0) override;
	uint8_t readInternalButtons(uint64_t* btn);

private:
	std::shared_ptr<FFBShifterEffects> effects_calc = std::make_shared<FFBShifterEffects>();

};
#endif
#endif /* USEREXTENSIONS_SRC_FFBSHIFTER_H_ */
