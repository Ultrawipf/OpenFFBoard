/*
 * EffectsCalculator.h
 *
 *  Created on: 27.01.2021
 *      Author: Yannick / Jon Lidgard
 */

#ifndef EFFECTSCALCULATOR_H_
#define EFFECTSCALCULATOR_H_

//#include "ClassChooser.h"
#include "ffb_defs.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include <vector>
//#include "hid_cmd_defs.h"

class Axis;
struct metric_t;

// default effect gains
struct effect_gain_t {
	uint8_t friction = 127;
	uint8_t spring = 64;
	uint8_t damper = 127;
	uint8_t inertia = 127;
};

enum class EffectsCalculator_commands : uint32_t {
	ffbfiltercf,ffbfiltercf_q,effects,spring,friction,damper,inertia
};

class EffectsCalculator: public PersistentStorage, public CommandHandler {
public:
	EffectsCalculator();
	virtual ~EffectsCalculator();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	const ClassType getClassType() override {return ClassType::Internal;};

	void saveFlash();
	void restoreFlash();

//	virtual bool processHidCommand(HID_Custom_Data_t* data);
	bool isActive();
	void setActive(bool active);
	void calculateEffects(std::vector<std::unique_ptr<Axis>> &axes);
	virtual void setFilters(FFB_Effect* effect);
	void setGain(uint8_t gain);
	uint8_t getGain();
	void setCfFilter(uint32_t f,uint8_t q); // Set output filter frequency
	void logEffectType(uint8_t type);

	//virtual ParseStatus command(ParsedCommand_old *cmd, std::string *reply);
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	virtual std::string getHelpstring() { return "Controls internal FFB effects"; }

	void setEffectsArray(FFB_Effect* pEffects);
	FFB_Effect* effects = nullptr; // ptr to effects array in HidFFB

protected:

private:
// Filters
	bool effects_active = false; // was ffb_active
	uint8_t global_gain = 0xff;
	float damper_f = 30 , damper_q = 0.4;
	float friction_f = 50 , friction_q = 0.2; //50 0.2
	float inertia_f = 15 , inertia_q = 0.2;
	const uint32_t calcfrequency = 1000; // HID frequency 1khz
	uint32_t cfFilter_f = calcfrequency/2; // 500 = off
	uint8_t cfFilter_q = 70; // User settable. q * 10
	const float cfFilter_qfloatScaler = 0.01;

	// Rescale factor for conditional effect to boost or decrease the intensity
	const float spring_scaler = 4.0f;
	const float friction_scaler = 0.2f;
	const float damper_scaler = 2.0f;
	const float inertia_scaler = 200.0f;
	const int frictionPctSpeedToRampup = 5;										// define the max value of the range (0..5% of maxspeed) where torque is rampup on friction
	const float speedRampupPct = (frictionPctSpeedToRampup / 100.0) * 32767;	// compute the normalizedSpeed of pctToRampup factor

	effect_gain_t gain;

	uint32_t effects_used = 0;

	int32_t calcComponentForce(FFB_Effect *effect, int32_t forceVector, std::vector<std::unique_ptr<Axis>> &axes, uint8_t axis);
	int32_t calcNonConditionEffectForce(FFB_Effect* effect);
	int32_t calcConditionEffectForce(FFB_Effect *effect, float metric, uint8_t gain, uint8_t idx, float scale, float angle_ratio);
	int32_t applyEnvelope(FFB_Effect *effect, int32_t value);
	std::string listEffectsUsed();
};
#endif /* EFFECTSCALCULATOR_H_ */
