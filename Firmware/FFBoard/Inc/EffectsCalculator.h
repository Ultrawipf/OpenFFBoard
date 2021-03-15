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
struct effect_gain_t;
struct metric_t;

class EffectsCalculator: public PersistentStorage, public CommandHandler {
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
	void calculateEffects(std::vector<Axis*> axes);
	virtual void setFilters(FFB_Effect* effect);
	void setGain(uint8_t gain);
	uint8_t getGain();
	void setCfFilter(uint32_t f); // Set output filter frequency
	float getCfFilterFreq();

	ParseStatus command(ParsedCommand *cmd, std::string *reply);
		virtual std::string getHelpstring() { return "\nEffect commands: ffbfiltercf.\n"; }

	void setEffectsArray(FFB_Effect* pEffects);
	FFB_Effect* effects = nullptr; // ptr to effects array in HidFFB

protected:

private:
// Filters
	bool effects_active = false; // was ffb_active
	uint8_t global_gain = 0xff;
	float damper_f = 50 , damper_q = 0.2;
	float friction_f = 50 , friction_q = 0.2;
	float inertia_f = 50 , inertia_q = 0.2;
	uint32_t cfFilter_f = calcfrequency/2; // 500 = off
	const float cfFilter_q = 0.8;
	const uint32_t calcfrequency = 1000; // HID frequency 1khz

	int32_t calculateForce(FFB_Effect* effect, metric_t* metrics, effect_gain_t* gain, uint8_t axis, uint8_t axisCount);
	int32_t calcCondition(FFB_Effect *effect, int16_t metric, uint8_t gain, bool useDir,
			uint8_t idx, float scale, float angle_ratio);
	int32_t applyEnvelope(FFB_Effect *effect, int32_t value);
};
#endif /* EFFECTSCALCULATOR_H_ */
