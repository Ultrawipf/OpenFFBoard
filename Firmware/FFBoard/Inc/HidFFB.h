/*
 * HidFFB.h
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#ifndef HIDFFB_H_
#define HIDFFB_H_

#include <UsbHidHandler.h>
#include "ffb_defs.h"
#include "PersistentStorage.h"
#include "Filters.h"
#include "EffectsCalculator.h"
#include "FastAvg.h"

#define X_AXIS_ENABLE 1
#define Y_AXIS_ENABLE 2
#define Z_AXIS_ENABLE 4
#define DIRECTION_ENABLE(AXES) (1 << AXES)

/**
 * @brief This class handles the Force Feedback (FFB) HID communication.
 * It inherits from UsbHidHandler to process HID reports and from EffectsControlItf to control FFB effects.
 * It is responsible for parsing HID FFB reports from the host and managing the lifecycle of FFB effects.
 */
class HidFFB: public UsbHidHandler, public EffectsControlItf {
public:
	/**
	 * @brief Construct a new HidFFB object.
	 * @param ec A shared pointer to the EffectsCalculator instance.
	 * @param axisCount The number of axes to be controlled.
	 */
	HidFFB(std::shared_ptr<EffectsCalculator> ec,uint8_t axisCount);
	virtual ~HidFFB();

	/**
	 * @brief Handles outgoing HID reports from the host (PC -> Device). This is the main entry point for FFB commands.
	 * @param report_id The report ID.
	 * @param report_type The type of the report.
	 * @param buffer A pointer to the report data.
	 * @param bufsize The size of the report data.
	 * @override from UsbHidHandler
	 */
	void hidOut(uint8_t report_id, hid_report_type_t report_type,const uint8_t* buffer, uint16_t bufsize) override;

	/**
	 * @brief Handles incoming HID GET_FEATURE requests from the host (Device -> PC).
	 * Used for status reports like block load and PID pool.
	 * @param report_id The report ID.
	 * @param report_type The type of the report.
	 * @param buffer A pointer to the buffer where the report data should be stored.
	 * @param reqlen The requested length of the report.
	 * @return The actual length of the report.
	 * @override from UsbHidHandler
	 */
	uint16_t hidGet(uint8_t report_id, hid_report_type_t report_type,uint8_t* buffer, uint16_t reqlen) override;

	/**
	 * @brief Checks if the FFB is active.
	 * @return true if FFB is active, false otherwise.
	 * @override from EffectsControlItf
	 */
	bool getFfbActive() override;

	/**
	 * @brief Sends a HID report to the host.
	 * @param report A pointer to the report data.
	 * @param len The length of the report data.
	 * @return true if the report was sent successfully, false otherwise.
	 */
	static bool HID_SendReport(uint8_t *report,uint16_t len);

	/**
	 * @brief Resets all FFB effects.
	 * @override from EffectsControlItf
	 */
	void reset_ffb() override;

	/**
	 * @brief Starts the FFB.
	 * @override from EffectsControlItf
	 */
	void start_FFB() override;

	/**
	 * @brief Stops the FFB.
	 * @override from EffectsControlItf
	 */
	void stop_FFB() override;

	/**
	 * @brief Sets the FFB state (active/inactive).
	 * @param state The new state of the FFB.
	 * @override from EffectsControlItf
	 */
	void set_FFB(bool state) override;

	/**
	 * @brief Sets the global gain for all FFB effects.
	 * @param gain The new gain value (0-255).
	 * @override from EffectsControlItf
	 */
	void set_gain(uint8_t gain) override;
	
	/**
	 * @brief Sends a status report for a specific effect to the host.
	 * @param effect The ID of the effect.
	 */
	void sendStatusReport(uint8_t effect);

	/**
	 * @brief Sets the direction enable mask for the axes.
	 * This mask determines how direction parameters in HID reports are interpreted.
	 * @param mask The new direction enable mask.
	 */
	void setDirectionEnableMask(uint8_t mask);
	void updateSamplerate(float newSamplerate);

private:
	// HID processing methods
	std::shared_ptr<EffectsCalculator> effects_calc; //!< A shared pointer to the EffectsCalculator instance.
	std::array<FFB_Effect,EffectsCalculator::max_effects>& effects; //!< A reference to the array of FFB effects, managed by EffectsCalculator, Must be passed in constructor.

	/**
	 * @brief Handles the "Create New Effect" HID report. Allocates a new effect.
	 * @param effect Pointer to the HID report data.
	 */
	void new_effect(FFB_CreateNewEffect_Feature_Data_t* effect);

	/**
	 * @brief Frees an effect slot. This is called when a HID_ID_BLKFRREP is received.
	 * @param id The block index of the effect to free.
	 */
	void free_effect(uint16_t id);

	/**
	 * @brief Handles FFB control commands (Enable, Disable, Stop, Reset, etc.).
	 * @param cmd The control command byte.
	 */
	void ffb_control(uint8_t cmd);

	/**
	 * @brief Handles the "Set Effect" HID report. Configures a previously created effect.
	 * @param effect Pointer to the HID report data.
	 */
	void set_effect(FFB_SetEffect_t* effect);

	/**
	 * @brief Handles the "Set Condition" HID report (Spring, Damper, Friction, Inertia).
	 * @param cond Pointer to the HID report data.
	 */
	void set_condition(FFB_SetCondition_Data_t* cond);

	/**
	 * @brief Handles the "Set Envelope" HID report.
	 * @param report Pointer to the HID report data.
	 */
	void set_envelope(FFB_SetEnvelope_Data_t* report);

	/**
	 * @brief Handles the "Set Ramp" HID report.
	 * @param report Pointer to the HID report data.
	 */
	void set_ramp(FFB_SetRamp_Data_t* report);

	/**
	 * @brief Handles the "Set Constant Force" HID report.
	 * @param effect Pointer to the HID report data.
	 */
	void set_constant_effect(FFB_SetConstantForce_Data_t* effect);

	/**
	 * @brief Handles the "Set Periodic" HID report (Sine, Square, etc.).
	 * @param report Pointer to the HID report data.
	 */
	void set_periodic(FFB_SetPeriodic_Data_t* report);

	/**
	 * @brief Handles the "Effect Operation" HID report (Start, Stop, Start Solo).
	 * @param report Pointer to the HID report data.
	 */
	void set_effect_operation(FFB_EffOp_Data_t* report);

	/**
	 * @brief Sets the default filters for a new effect based on its type.
	 * @param effect Pointer to the effect to configure.
	 */
	void set_filters(FFB_Effect* effect);

	uint8_t directionEnableMask; //!< Mask to enable/disable directions for axes. Adjusted based on axis count.
	uint16_t used_effects = 0; //!< The number of currently active effects.
	bool ffb_active = false; //!< Flag indicating if FFB is globally active.
	FFB_BlockLoad_Feature_Data_t blockLoad_report; //!< HID report structure for block load status.
	FFB_PIDPool_Feature_Data_t pool_report; //!< HID report structure for PID pool status.

	reportFFB_status_t reportFFBStatus; //!< HID report structure for general FFB status.

	uint8_t axisCount; //!< The number of axes supported by this FFB instance.
};

#endif /* HIDFFB_H_ */
