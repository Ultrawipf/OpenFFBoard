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
 * @brief This class handles HID FFB reports from the USB host.
 * It decodes the reports and updates the effects in the EffectsCalculator.
 */
class HidFFB: public UsbHidHandler, public EffectsControlItf {
public:
	/**
	 * @brief Construct a new HidFFB object.
	 * @param ec Shared pointer to the EffectsCalculator.
	 * @param axisCount Number of FFB axes.
	 */
	HidFFB(std::shared_ptr<EffectsCalculator> ec,uint8_t axisCount);
	virtual ~HidFFB();

	/**
	 * @brief Handles HID OUT reports.
	 * @override from UsbHidHandler
	 */
	void hidOut(uint8_t report_id, hid_report_type_t report_type,const uint8_t* buffer, uint16_t bufsize) override;
	/**
	 * @brief Handles HID GET reports.
	 * @override from UsbHidHandler
	 */
	uint16_t hidGet(uint8_t report_id, hid_report_type_t report_type,uint8_t* buffer, uint16_t reqlen) override;

	/**
	 * @brief Checks if FFB is active.
	 * @return true if active, false otherwise.
	 */
	bool getFfbActive() override;
	/**
	 * @brief Sends a HID report.
	 */
	static bool HID_SendReport(uint8_t *report,uint16_t len);

	/**
	 * @brief Resets all FFB effects.
	 */
	void reset_ffb() override;
	/**
	 * @brief Starts FFB.
	 */
	void start_FFB() override;
	/**
	 * @brief Stops FFB.
	 */
	void stop_FFB() override;
	/**
	 * @brief Enables or disables FFB.
	 * @param state true to enable, false to disable.
	 */
	void set_FFB(bool state) override;
	/**
	 * @brief Sets the global FFB gain.
	 * @param gain The gain value (0-255).
	 */
	void set_gain(uint8_t gain) override;
	
	/**
	 * @brief Sends a status report for a specific effect.
	 */
	void sendStatusReport(uint8_t effect);
	/**
	 * @brief Sets the direction enable mask.
	 */
	void setDirectionEnableMask(uint8_t mask);
	/**
	 * @brief Updates the internal sample rate.
	 * @param newSamplerate The new sample rate in Hz.
	 */
	void updateSamplerate(float newSamplerate) override;

private:
	// HID
	std::shared_ptr<EffectsCalculator> effects_calc;
	std::array<FFB_Effect,EffectsCalculator::max_effects>& effects; //!< Reference to the effects array in EffectsCalculator.
	
	/**
	 * @brief Handles the creation of a new effect.
	 */
	void new_effect(FFB_CreateNewEffect_Feature_Data_t* effect);
	/**
	 * @brief Handles freeing an effect slot.
	 */
	void free_effect(uint16_t id);
	/**
	 * @brief Decodes the FFB control command.
	 */
	void ffb_control(uint8_t cmd);
	/**
	 * @brief Decodes the set effect report.
	 */
	void set_effect(FFB_SetEffect_t* effect);
	/**
	 * @brief Decodes the set condition report.
	 */
	void set_condition(FFB_SetCondition_Data_t* cond);
	/**
	 * @brief Decodes the set envelope report.
	 */
	void set_envelope(FFB_SetEnvelope_Data_t* report);
	/**
	 * @brief Decodes the set ramp report.
	 */
	void set_ramp(FFB_SetRamp_Data_t* report);
	/**
	 * @brief Decodes the set constant force report.
	 */
	void set_constant_effect(FFB_SetConstantForce_Data_t* effect);
	/**
	 * @brief Decodes the set periodic report.
	 */
	void set_periodic(FFB_SetPeriodic_Data_t* report);
	/**
	 * @brief Decodes the effect operation report (start/stop/pause).
	 */
	void set_effect_operation(FFB_EffOp_Data_t* report);


	/**
	 * @brief Updates the filters for an effect.
	 */
	void set_filters(FFB_Effect* effect);

	uint8_t directionEnableMask; //!< Mask for enabled directions.
	uint16_t used_effects = 0;   //!< Number of currently used effect slots.
	bool ffb_active = false;      //!< true if FFB is active.
	FFB_BlockLoad_Feature_Data_t blockLoad_report; //!< HID Feature report: Block Load.
	FFB_PIDPool_Feature_Data_t pool_report;           //!< HID Feature report: PID Pool.

	reportFFB_status_t reportFFBStatus; //!< Holds the current FFB status.

	uint8_t axisCount; //!< Number of FFB axes.
};

#endif /* HIDFFB_H_ */
