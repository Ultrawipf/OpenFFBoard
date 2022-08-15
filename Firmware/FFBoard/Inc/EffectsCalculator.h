/*
 * EffectsCalculator.h
 *
 *  Created on: 27.01.2021
 *      Author: Yannick / Jon Lidgard / Vincent Manoukian
 */

#ifndef EFFECTSCALCULATOR_H_
#define EFFECTSCALCULATOR_H_

//#include "ClassChooser.h"
#include "ffb_defs.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include <vector>
//#include "hid_cmd_defs.h"

#define EFFECT_THREAD_MEM 128
#define EFFECT_THREAD_PRIO 20 // low priority for stat

#define INTERNAL_SCALER_DAMPER 40
#define INTERNAL_SCALER_FRICTION 45
#define INTERNAL_SCALER_INERTIA 4

class Axis;
struct metric_t;

// default effect gains
struct effect_gain_t {
	uint8_t friction = 254;
	uint8_t spring = 64;
	uint8_t damper = 64;
	uint8_t inertia = 127;
};

struct effect_scaler_t {
	float friction = 1.0; //0.4 * 40;
	float spring = 16.0;
	float damper = 4.0; //2 * 40 * 2
	float inertia = 2.0;//0.5 * 40;
};

struct effect_biquad_t {
	biquad_constant_t constant	= { 500, 70 };
	biquad_constant_t friction 	= { 50, 20 };
	biquad_constant_t damper 	= { 30, 40 };
	biquad_constant_t inertia	= { 15, 20 };
};

struct effect_stat_t {
	int16_t current=0;
	int16_t max=0;
	uint16_t nb=0;
};

enum class EffectsCalculator_commands : uint32_t {
	ffbfiltercf,ffbfiltercf_q,effects,spring,friction,damper,inertia,
	damper_f, damper_q, friction_f, friction_q, inertia_f, inertia_q,
	frictionPctSpeedToRampup,
	monitorEffect, effectsDetails, effectsForces,
};

class EffectsCalculator: public PersistentStorage,
						 public CommandHandler,
						 cpp_freertos::Thread {
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
	void logEffectType(uint8_t type,bool remove = false);
	void setDirectionEnableMask(uint8_t mask);
	void calcStatsEffectType(uint8_t type, int16_t force);
	void logEffectState(uint8_t type,uint8_t state);

	//virtual ParseStatus command(ParsedCommand_old *cmd, std::string *reply);
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	virtual std::string getHelpstring() { return "Controls internal FFB effects"; }

	void setEffectsArray(FFB_Effect* pEffects);
	FFB_Effect* effects = nullptr; // ptr to effects array in HidFFB

	// Thread impl
	void Run();


protected:

private:
	uint8_t directionEnableMask = 0;
	// Filters
	effect_biquad_t filter;
	const uint32_t calcfrequency = 1000; 		// HID frequency 1khz
	const float qfloatScaler = 0.01;

	// Rescale factor for conditional effect to boost or decrease the intensity
	uint8_t global_gain = 0xff;
	effect_gain_t gain;
	effect_scaler_t scaler;
	uint8_t frictionPctSpeedToRampup = 25;	// define the max value of the range (0..5% of maxspeed) where torque is rampup on friction

	// FFB status
	bool effects_active = false; // was ffb_active
	uint32_t effects_used = 0;
	std::array<effect_stat_t,12> effects_stats; // [0..12 effect types]
	bool isMonitorEffect = false;

	int32_t calcComponentForce(FFB_Effect *effect, int32_t forceVector, std::vector<std::unique_ptr<Axis>> &axes, uint8_t axis);
	int32_t calcNonConditionEffectForce(FFB_Effect* effect);
	float speedRampupPct();
	int32_t calcConditionEffectForce(FFB_Effect *effect, float metric, uint8_t gain, uint8_t idx, float scale, float angle_ratio);
	int32_t getEnvelopeMagnitude(FFB_Effect *effect);
	std::string listEffectsUsed(bool details = false);
	//std::string listForceEffects();
	void checkFilterCoeff(biquad_constant_t *filter, uint32_t freq,uint8_t q);
	void updateFilterSettingsForEffects(uint8_t type_effect);
};
#endif /* EFFECTSCALCULATOR_H_ */
