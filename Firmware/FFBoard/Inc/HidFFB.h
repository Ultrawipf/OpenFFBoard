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


class HidFFB: public UsbHidHandler {
public:
	HidFFB(EffectsCalculator &ec);
	virtual ~HidFFB();

	void hidOut(uint8_t report_id, hid_report_type_t report_type,const uint8_t* buffer, uint16_t bufsize) override;
	uint16_t hidGet(uint8_t report_id, hid_report_type_t report_type,uint8_t* buffer, uint16_t reqlen) override;

	uint32_t getRate(); // Returns an estimate of the hid effect update speed in hz
	uint32_t getConstantForceRate(); // Returns an estimate of the constant force effect update rate in hz
	bool getFfbActive();
	static bool HID_SendReport(uint8_t *report,uint16_t len);

	void reset_ffb();
	void start_FFB();
	void stop_FFB();
	void set_FFB(bool state);
	void set_gain(uint8_t gain);
	
	void sendStatusReport(uint8_t effect);

	std::array<FFB_Effect,EffectsCalculator::max_effects>& effects; // Must be passed in constructor
private:
	// HID
	EffectsCalculator* effects_calc = nullptr;
	uint8_t find_free_effect(uint8_t type);
	void new_effect(FFB_CreateNewEffect_Feature_Data_t* effect);
	void free_effect(uint16_t id);
	void ffb_control(uint8_t cmd);
	void set_effect(FFB_SetEffect_t* effect);
	void set_condition(FFB_SetCondition_Data_t* cond);
	void set_envelope(FFB_SetEnvelope_Data_t* report);
	void set_ramp(FFB_SetRamp_Data_t* report);
	void set_constant_effect(FFB_SetConstantForce_Data_t* effect);
	void set_periodic(FFB_SetPeriodic_Data_t* report);
	void set_effect_operation(FFB_EffOp_Data_t* report);


	void set_filters(FFB_Effect* effect);


	uint16_t used_effects = 0;
	bool ffb_active = false;
	FFB_BlockLoad_Feature_Data_t blockLoad_report;
	FFB_PIDPool_Feature_Data_t pool_report;

	reportFFB_status_t reportFFBStatus;
	FastAvg<float,20> hidPeriodAvg;
	FastAvg<float,20> cfUpdatePeriodAvg;

	uint32_t lastOut = 0;
	uint32_t lastCfUpdate = 0;
};

#endif /* HIDFFB_H_ */
