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
#include "FastAvg.h"

#define EFFECT_THREAD_MEM 128
#define EFFECT_THREAD_PRIO 20 // low priority for stat

#define INTERNAL_SCALER_DAMPER 40
#define INTERNAL_SCALER_FRICTION 45
#define INTERNAL_SCALER_INERTIA 4

#define CUSTOM_PROFILE_ID 1

class Axis;
struct metric_t;

enum class ReconFilterMode : uint8_t {
	NO_RECONSTRUCTION = 0,
    LINEAR_INTERPOLATION = 1, 	// More responsive, attempts to match the game's signal precisely. Can feel "grainy" if slew rate is high.
	SPLINE_CUBIC_NATURAL = 2,	// Highest fidelity, smooth curve through last 4 points. More CPU intensive.
    SPLINE_CUBIC_HERMITE = 3  	// Mixed solution, good fidelity and optimized timing
};

/**
 * @brief Default gains for conditional effects.
 */
struct effect_gain_t {
	uint8_t friction = 254;
	uint8_t spring = 64;
	uint8_t damper = 64;
	uint8_t inertia = 127;
};

/**
 * @brief Scaler values for conditional effects to adjust their intensity.
 */
struct effect_scaler_t {
	float friction = 1.0; //0.4 * 40;
	float spring = 16.0;
	float damper = 4.0; //2 * 40 * 2
	float inertia = 2.0;//0.5 * 40;
};

/**
 * @brief Biquad filter coefficients for various effects.
 */
struct effect_biquad_t {
	biquad_constant_t constant	= { 500, 70 };
	biquad_constant_t friction 	= { 50, 20 };
	biquad_constant_t damper 	= { 30, 40 };
	biquad_constant_t inertia	= { 15, 20 };
};

/**
 * @brief Statistics for each effect type.
 */
struct effect_stat_t {
	std::array<int16_t,MAX_AXIS> current={0}; //!< Current force value.
	std::array<int16_t,MAX_AXIS> max={0};     //!< Maximum force value since reset.
	uint16_t nb=0;                           //!< Number of times this effect has been used.
};

enum class EffectsCalculator_commands : uint32_t {
	ffbfiltercf,ffbfiltercf_q,effects,spring,friction,damper,inertia,
	damper_f, damper_q, friction_f, friction_q, inertia_f, inertia_q, filterProfileId,
	frictionPctSpeedToRampup,
	monitorEffect, effectsDetails, effectsForces,
	reconFilterMode
};

/**
 * @brief This class is responsible for calculating the forces of all active FFB effects.
 * It runs in its own thread to perform the calculations.
 */
class EffectsCalculator: public PersistentStorage,
						 public CommandHandler,
						 cpp_freertos::Thread {
public:
	EffectsCalculator();
	virtual ~EffectsCalculator();

	static ClassIdentifier info; //!< Static class identifier.
	/**
	 * @brief Returns the class identifier.
	 * @return The class identifier.
	 */
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
	 * @param active The new active state.
	 */
	void setActive(bool active);

	/**
	 * @brief This is the main calculation method. It iterates through all active effects and sums up the forces for each axis.
	 * @param axes A reference to the vector of axes.
	 */
	void calculateEffects(std::vector<std::unique_ptr<Axis>> &axes);

	/**
	 * @brief Sets the filters for a specific effect.
	 * @param effect A pointer to the effect.
	 */
	virtual void setFilters(FFB_Effect* effect);

	/**
	 * @brief Sets the global gain for all effects.
	 * @param gain The new gain value.
	 */
	void setGain(uint8_t gain);

	/**
	 * @brief Gets the global gain.
	 * @return The current global gain value.
	 */
	uint8_t getGain();

	/**
	 * @brief Logs the usage of an effect type for statistics.
	 * @param type The effect type.
	 * @param remove True to decrement the usage count, false to increment.
	 */
	void logEffectType(uint8_t type,bool remove = false);

	/**
	 * @brief Calculates and stores statistics for a given effect type.
	 * @param type The effect type.
	 * @param force The calculated force.
	 * @param axis The axis index.
	 */
	void calcStatsEffectType(uint8_t type, int32_t force,uint8_t axis);

	/**
	 * @brief Logs the state of an effect (start/stop).
	 * @param type The effect type.
	 * @param state The new state.
	 */
	void logEffectState(uint8_t type,uint8_t state);

	/**
	 * @brief Resets the statistics of active effects.
	 * @param reinit If true, re-initializes the stats based on currently active effects.
	 */
	void resetLoggedActiveEffects(bool reinit);

	/**
	 * @brief Finds a free slot for a new effect.
	 * @param type The type of the effect.
	 * @return The index of the free slot, or -1 if no slot is available.
	 */
	int32_t find_free_effect(uint8_t type);

	/**
	 * @brief Frees an effect slot.
	 * @param idx The index of the effect to free.
	 */
	void free_effect(uint16_t idx);

	/**
     * @brief Met à jour les tampons d'interpolation pour un effet non-conditionnel.
     * Appelé par le handler HID (ex: 60Hz).
     * @param effect Pointeur vers l'effet à modifier.
     * @param new_magnitude Nouvelle magnitude/amplitude cible.
     * @param new_offset Nouvel offset cible (utilisé uniquement si is_periodic est true).
     * @param is_periodic Si true, met à jour les deux tampons (mag + offset).
     */
    void pushReconFilterValue(FFB_Effect* effect, float new_magnitude, float new_offset, bool is_periodic);

	/**
	 * @brief Handles command line interface commands for the effects calculator.
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
	 * @brief The main loop for the effects calculator thread.
	 * @override from cpp_freertos::Thread
	 */
	void Run() override;

	void updateSamplerate(float newSamplerate); // Must be called if update rate is changed to update filters and effects


protected:

private:
	// Filters
	effect_biquad_t filter[2];		//!< Filter coefficients for CFFilter, default (0) and custom (1) profiles.
	uint8_t filterProfileId = 0;	//!< Currently active filter profile (0 or 1).
	uint32_t calcfrequency = 1000; //!< Calculation frequency in Hz (matches HID rate 1khz).
	const float qfloatScaler = 0.01;	 //!< Scaler for Q factor.

	// Gains and scalers
	uint8_t global_gain = 0xff;		//!< Global gain for all effects.
	effect_gain_t gain;				//!< Per-effect type gains.
	effect_scaler_t scaler;			//!< Per-effect type intensity scalers.
	uint8_t frictionPctSpeedToRampup = 25;	//!< Speed percentage for friction ramp-up. : define the max value of the range (0..5% of maxspeed) where torque is rampup on friction

	// FFB status
	bool effects_active = false;	//!< True if FFB is globally active.
	uint32_t effects_used = 0;		//!< Bitmask of effect types used since reset.
	std::array<effect_stat_t,12> effects_stats; //!< Statistics for each effect type, [0..12 effect types].
	std::array<effect_stat_t,12> effects_statslast; //!< Statistics from the previous cycle, [0..12 effect types].
	bool isMonitorEffect = false;	//!< Flag to enable effect monitoring.

	// Reconstruction filter
	ReconFilterMode reconFilterMode = ReconFilterMode::NO_RECONSTRUCTION;	//!< Current reconstruction filter mode.

	/**
	 * @brief Internal method to push a new value into the reconstruction filter.
	 * @param state Pointer to the reconstruction filter state.
	 * @param newValue The new value to push into the filter.
	 */
	void pushReconParam(ReconFilterState* state, float newValue);

	/**
	 * @brief Retrieves the value from the linear reconstruction filter.
	 * @param state Pointer to the reconstruction filter state.
	 * @param fallbackValue Value to return if the filter is not ready.
	 * @return The interpolated value from the filter.
	 */
	float getReconstructedvalue(ReconFilterState* state, float fallbackValue);

	/**
	 * @brief Calculates the force for a single component of an effect on a specific axis.
	 * Dispatches between conditional and non-conditional effects.
	 */
	int32_t calculateEffectForceOnAxis(FFB_Effect *effect, int32_t forceVector, std::vector<std::unique_ptr<Axis>> &axes, uint8_t axis);

	/**
	 * @brief Calculates the base force for non-conditional effects (Constant, Ramp, Periodic).
	 */
	int32_t calcNonConditionEffectForce(FFB_Effect* effect);

	/**
	 * @brief Calculates the speed threshold for friction ramp-up.
	 */
	float speedRampupPct();

	/**
	 * @brief Calculates the force for conditional effects (Spring, Damper, Inertia, Friction).
	 */
	int32_t calcConditionEffectForce(FFB_Effect *effect, float metric, uint8_t gain, uint8_t idx, float scale, float angle_ratio);

	/**
	 * @brief Calculates the magnitude of an effect with an envelope.
	 */
	int32_t getEnvelopeMagnitude(FFB_Effect *effect, int32_t baseMagnitude);

	/**
	 * @brief Generates a string listing the effects used.
	 */
	std::string listEffectsUsed(bool details = false,uint8_t axis = 0);

	/**
	 * @brief Validates and sanitizes filter coefficients.
	 */
	void checkFilterCoeff(biquad_constant_t *filter, uint32_t freq,uint8_t q);

	/**
	 * @brief Updates the filters for all active effects of a specific type.
	 */
	void updateFilterSettingsForEffects(uint8_t type_effect);
};

/**
 * Helper interface class for common effects calculator related control functions
 */
/**
 * @brief Helper interface class for common effects calculator related control functions.
 * This class provides a common interface for controlling FFB effects.
 */
class EffectsControlItf{
public:
	/**
	 * @brief Enables or disables FFB.
	 * @param state The new state of the FFB.
	 */
	virtual void set_FFB(bool state) = 0;

	/**
	 * @brief Stops the FFB.
	 */
	virtual void stop_FFB(){set_FFB(false);};

	/**
	 * @brief Starts the FFB.
	 */
	virtual void start_FFB(){set_FFB(true);};

	/**
	 * @brief Resets all FFB effects.
	 */
	virtual void reset_ffb() = 0;

	/**
	 * @brief Returns an estimate of the constant force effect update rate in Hz.
	 * @return The estimated update rate.
	 */
	virtual uint32_t getConstantForceRate();

	/**
	 * @brief Returns an estimate of the overall effect update speed in Hz.
	 * @return The estimated update rate.
	 */
	virtual uint32_t getRate();

	/**
	 * @brief Checks if FFB is active.
	 * @return true if FFB is active, false otherwise.
	 */
	virtual bool getFfbActive() = 0;

	/**
	 * @brief Sets the global gain for all effects.
	 * @param gain The new gain value.
	 */
	virtual void set_gain(uint8_t gain) = 0;

	/**
	 * @brief To be called when a constant force update event occurs. Used for rate calculation.
	 */
	virtual void cfUpdateEvent();

	/**
	 * @brief To be called when any FFB update event occurs. Used for rate calculation.
	 */
	virtual void fxUpdateEvent();
	virtual void updateSamplerate(float newSamplerate) = 0; // Should be called when update loop rate is changed

private:
	FastMovingAverage<float> fxPeriodAvg{5};
	FastMovingAverage<float> cfUpdatePeriodAvg{5};

	uint32_t lastFxUpdate = 0;	//!< Timestamp of the last FFB update.
	uint32_t lastCfUpdate = 0;	//!< Timestamp of the last constant force update.
};

#endif /* EFFECTSCALCULATOR_H_ */
