/*
 * SerialFFB.h
 *
 *  Created on: 30.11.2022
 *      Author: Yannick
 */

#ifndef SRC_SERIALFFB_H_
#define SRC_SERIALFFB_H_

#include "CommandHandler.h"
#include "EffectsCalculator.h"

/**
 * Helper class to manage the effectscalculator effects via commands directly
 */
class SerialFFB : public CommandHandler, public EffectsControlItf{

	enum class SerialEffects_commands : uint32_t {
		ffbstate,ffbreset,newEffect,fxtype,fxmagnitude,fxstate,fxperiod,fxduration,fxoffset,fxdeadzone,fxsat,fxcoeff,fxaxisgain
	};

public:
	SerialFFB(std::shared_ptr<EffectsCalculator> ec,uint8_t instance=0);
	virtual ~SerialFFB();

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	const ClassIdentifier getInfo();

	bool getFfbActive();

	void reset_ffb();
	void set_FFB(bool state);
	void set_gain(uint8_t gain);

	int32_t newEffect(uint8_t effectType);
	void setMagnitude(uint8_t idx,int16_t magnitude);

	void setEffectState(uint8_t id, bool state);

private:
	static ClassIdentifier info;
	std::shared_ptr<EffectsCalculator> effects_calc;
	std::array<FFB_Effect,EffectsCalculator::max_effects> &effects; // Direct access to effects calculator effect array

	static constexpr FFB_Effect_Condition defaultCond = {
			.cpOffset = 0,
			.positiveCoefficient = 0x7fff,
			.negativeCoefficient = 0x7fff,
			.positiveSaturation = 0x7fff,
			.negativeSaturation = 0x7fff,
			.deadBand = 0
	};
};

#endif /* SRC_SERIALFFB_H_ */
