/*
 * EffectsCalculator.h
 *
 *  Created on: 27.01.2021
 *      Author: Yannick / Jon Lidgard / Vincent Manoukian
 */

#ifndef EFFECTSCALCULATOR_H_
#define EFFECTSCALCULATOR_H_

#include "ffb_defs.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include <vector>
#include "FastAvg.h"

#define EFFECT_THREAD_MEM 128
#define EFFECT_THREAD_PRIO 20 // low priority for stat

#define INTERNAL_SCALER_DAMPER 40
#define INTERNAL_SCALER_FRICTION 45
#define INTERNAL_SCALER_INERTIA 4

#define CUSTOM_PROFILE_ID 1

class Axis;
struct metric_t;

/**
 * @brief Default effect gains.
 */
struct effect_gain_t {
	uint8_t friction = 254;
	uint8_t spring = 64;
	uint8_t damper = 64;
	uint8_t inertia = 127;
};

/**
 * @brief Default effect scalers.
 */
struct effect_scaler_t {
	float friction = 1.0;
	float spring = 16.0;
	float damper = 4.0;
	float inertia = 2.0;
};

/**
 * @brief Biquad filter constants for different effect types.
 */
struct effect_biquad_t {
	biquad_constant_t constant      = { 500, 70 };
	biquad_constant_t friction      = { 50, 20 };
	biquad_constant_t damper        = { 30, 40 };
	biquad_constant_t inertia       = { 15, 20 };
};

/**
 * @brief Holds statistics for an effect type.
 */
struct effect_stat_t {
	std::array<int16_t,MAX_AXIS> current={0}; //!< Current force for each axis.
	std::array<int16_t,MAX_AXIS> max={0};     //!< Maximum force for each axis.
	uint16_t nb=0;                             //!< Number of samples.
};

enum class EffectsCalculator_commands : uint32_t {
	ffbfiltercf,ffbfiltercf_q,effects,spring,friction,damper,inertia,
	damper_f, damper_q, friction_f, friction_q, inertia_f, inertia_q, filterProfileId,
	frictionPctSpeedToRampup,
	monitorEffect, effectsDetails, effectsForces,
};

/**
 * @brief This class calculates the final torque for each axis based on the received HID FFB effects.
 */
class EffectsCalculator: public PersistentStorage,
						 public CommandHandler,
						 cpp_freertos::Thread {
public:
	EffectsCalculator();
	virtual ~EffectsCalculator();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	const ClassType getClassType() override {return ClassType::Internal;};

	/**
	 * @brief Saves effect settings to flash memory.
	 * @override from PersistentStorage
	 */
	void saveFlash() override;
	/**
	 * @brief Restores effect settings from flash memory.
	 * @override from PersistentStorage
	 */
	void restoreFlash() override;

	/**
	 * @brief Checks if the effects calculator is active.
	 * @return true if active, false otherwise.
	 */
	bool isActive();
	/**
	 * @brief Sets the active state of the effects calculator.
	 * @param active true to activate, false to deactivate.
	 */
	void setActive(bool active);
	/**
	 * @brief Calculates the combined force of all active effects for each axis.
	 * @param axes A vector of unique pointers to the axis objects.
	 */
	void calculateEffects(std::vector<std::unique_ptr<Axis>> &axes);
	/**
	 * @brief Sets the filters for a specific effect.
	 * @param effect A pointer to the FFB_Effect object.
	 */
	virtual void setFilters(FFB_Effect* effect);
	/**
	 * @brief Sets the global gain for all effects.
	 * @param gain The gain value (0-255).
	 */
	void setGain(uint8_t gain);
	/**
	 * @brief Gets the global gain.
	 * @return The global gain value.
	 */
	uint8_t getGain();
	/**
	 * @brief Logs an effect type as active or inactive.
	 * @param type The effect type ID.
	 * @param remove true to log as inactive, false as active.
	 */
	void logEffectType(uint8_t type,bool remove = false);
	/**
	 * @brief Calculates statistics for an effect type.
	 * @param type The effect type ID.
	 * @param force The calculated force value.
	 * @param axis The axis ID.
	 */
	void calcStatsEffectType(uint8_t type, int32_t force,uint8_t axis);
	/**
	 * @brief Logs the state of an effect (active, inactive, etc.).
	 * @param type The effect type ID.
	 * @param state The state value.
	 */
	void logEffectState(uint8_t type,uint8_t state);
	/**
	 * @brief Resets the log of active effects.
	 * @param reinit If true, reinitializes the log.
	 */
	void resetLoggedActiveEffects(bool reinit);

	/**
	 * @brief Finds a free effect slot in the effects array.
	 * @param type The effect type ID.
	 * @return The index of the free slot, or -1 if none found.
	 */
	int32_t find_free_effect(uint8_t type);
	/**
	 * @brief Frees an effect slot.
	 * @param idx The index of the slot to free.
	 */
	void free_effect(uint16_t idx);

	/**
	 * @brief Handles CLI commands for the effects calculator.
	 * @param cmd The parsed command.
	 * @param replies A vector of replies to be sent back.
	 * @return The status of the command execution.
	 * @override from CommandHandler
	 */
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;
	virtual std::string getHelpstring() { return "Controls internal FFB effects"; }

	static const uint32_t max_effects = MAX_EFFECTS;
	std::array<FFB_Effect,max_effects> effects; //!< Main effects storage array.

	/**
	 * @brief Main thread function for the effects calculator.
	 */
	void Run() override;

	/**
	 * @brief Updates the internal sample rate and recalculates filters.
	 * @param newSamplerate The new sample rate in Hz.
	 */
	void updateSamplerate(float newSamplerate);


protected:

private:
	// Filters
	effect_biquad_t filter[2];              //!< Filter profiles: 0 is default, CUSTOM_PROFILE_ID is custom.
	uint8_t filterProfileId = 0;            //!< Currently selected filter profile ID.
	uint32_t calcfrequency = 1000;          //!< Calculation frequency in Hz (default 1kHz).
	const float qfloatScaler = 0.01;        //!< Scaler for Q factor.

	// Intensity and scaling
	uint8_t global_gain = 0xff;             //!< Global gain for all effects.
	effect_gain_t gain;                     //!< Individual effect type gains.
	effect_scaler_t scaler;                 //!< Individual effect type scalers.
	uint8_t frictionPctSpeedToRampup = 25;  //!< Friction ramp-up threshold (percentage of max speed).

	// FFB status
	bool effects_active = false;            //!< true if FFB is active.
	uint32_t effects_used = 0;              //!< Mask of currently used effect slots.
	std::array<effect_stat_t,12> effects_stats;     //!< Statistics for each effect type (current cycle).
	std::array<effect_stat_t,12> effects_statslast; //!< Statistics from the previous cycle.
	bool isMonitorEffect = false;           //!< Flag for effect monitoring.

	/**
	 * @brief Calculates the force component for a specific effect on a specific axis.
	 */
	int32_t calcComponentForce(FFB_Effect *effect, int32_t forceVector, std::vector<std::unique_ptr<Axis>> &axes, uint8_t axis);
	/**
	 * @brief Calculates the force for a non-conditional effect (e.g., Constant Force, Sine).
	 */
	int32_t calcNonConditionEffectForce(FFB_Effect* effect);
	/**
	 * @brief Returns the speed ramp-up factor for friction.
	 */
	float speedRampupPct();
	/**
	 * @brief Calculates the force for a conditional effect (e.g., Spring, Damper, Friction).
	 */
	int32_t calcConditionEffectForce(FFB_Effect *effect, float metric, uint8_t gain, uint8_t idx, float scale, float angle_ratio);
	/**
	 * @brief Calculates the envelope magnitude for an effect.
	 */
	int32_t getEnvelopeMagnitude(FFB_Effect *effect);
	/**
	 * @brief Lists the currently used effects as a string.
	 */
	std::string listEffectsUsed(bool details = false,uint8_t axis = 0);
	/**
	 * @brief Checks and updates filter coefficients.
	 */
	void checkFilterCoeff(biquad_constant_t *filter, uint32_t freq,uint8_t q);
	/**
	 * @brief Updates filter settings for a specific effect type.
	 */
	void updateFilterSettingsForEffects(uint8_t type_effect);
};

/**
 * @brief Helper interface class for common effects calculator related control functions.
 */
class EffectsControlItf{
public:
	/**
	 * @brief Enables or disables FFB globally.
	 */
	virtual void set_FFB(bool state) = 0;
	virtual void stop_FFB(){set_FFB(false);};
	virtual void start_FFB(){set_FFB(true);};
	/**
	 * @brief Resets all FFB effects.
	 */
	virtual void reset_ffb() = 0;
	/**
	 * @brief Returns the estimated constant force update rate in Hz.
	 */
	virtual uint32_t getConstantForceRate();
	/**
	 * @brief Returns the estimated overall effect update speed in Hz.
	 */
	virtual uint32_t getRate();
	/**
	 * @brief Checks if FFB is active.
	 */
	virtual bool getFfbActive() = 0;
	/**
	 * @brief Sets the global FFB gain.
	 */
	virtual void set_gain(uint8_t gain) = 0;
	/**
	 * @brief Event handler for constant force updates.
	 */
	virtual void cfUpdateEvent();
	/**
	 * @brief Event handler for effect updates.
	 */
	virtual void fxUpdateEvent();
	/**
	 * @brief Updates the sample rate for filters and effects.
	 */
	virtual void updateSamplerate(float newSamplerate) = 0;

private:
	FastMovingAverage<float> fxPeriodAvg{5};          //!< Average period between effect updates.
	FastMovingAverage<float> cfUpdatePeriodAvg{5};    //!< Average period between constant force updates.

	uint32_t lastFxUpdate = 0; //!< Time of last effect update.
	uint32_t lastCfUpdate = 0; //!< Time of last constant force update.
};

#endif /* EFFECTSCALCULATOR_H_ */
