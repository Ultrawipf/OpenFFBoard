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
#include "HidCommandHandler.h"
#include <vector>
//#include "hid_cmd_defs.h"

class Axis;
struct metric_t;

struct effect_gain_t {
	uint8_t friction = 127;
	uint8_t spring = 255;
	uint8_t damper = 127;
	uint8_t inertia = 127;
};

class EffectsCalculator: public PersistentStorage, public CommandHandler,public HidCommandHandler {
public:
	EffectsCalculator();
	virtual ~EffectsCalculator();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

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

	virtual ParseStatus command(ParsedCommand *cmd, std::string *reply);
	virtual void processHidCommand(HID_Custom_Data_t* data);
	virtual std::string getHelpstring() { return "\nEffect commands: effects,ffbfiltercf,ffbfiltercf_q,spring,damper,friction,inertia.\n"; }

	void setEffectsArray(FFB_Effect* pEffects);
	FFB_Effect* effects = nullptr; // ptr to effects array in HidFFB

protected:

private:
// Filters
	bool effects_active = false; // was ffb_active
	uint8_t global_gain = 0xff;
	float damper_f = 500 , damper_q = 0.2;
	float friction_f = 50 , friction_q = 0.2; //50 0.2
	float inertia_f = 5 , inertia_q = 0.2;
	const uint32_t calcfrequency = 1000; // HID frequency 1khz
	uint32_t cfFilter_f = calcfrequency/2; // 500 = off
	uint8_t cfFilter_q = 70; // User settable
	const float cfFilter_qfloatScaler = 0.01;

	// Rescale factor for conditional effect to boos or slow them
	const float spring_scaler = 1.0f;
	const float friction_scaler = 1.0f;
	const float damper_scaler = 1.6f;
	const float inertia_scaler = 10.0f;
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
