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

class HidFFB: public UsbHidHandler, PersistentStorage {
public:
	HidFFB();
	virtual ~HidFFB();

	void hidOut(uint8_t* report);
	void hidGet(uint8_t id,uint16_t len,uint8_t** return_buf);
	int32_t calculateEffects(int32_t pos,uint8_t axis); //Axis: 1/2 pos: current position scaled from -0x7fff to 0x7fff
	bool idlecenter = true;

	float damper_f = 50 , damper_q = 0.2;
	BiquadType damper_type = BiquadType::lowpass;
	float friction_f = 50 , friction_q = 0.2;
	BiquadType friction_type = BiquadType::lowpass;
	const uint16_t calcfrequency = 1000;


	void setIdleSpringStrength(uint8_t spring);
	uint8_t getIdleSpringStrength();
	void setFrictionStrength(uint8_t strength);
	uint8_t getFrictionStrength();

	uint32_t getRate(); // Returns an estimate of the hid effect update speed in hz
	bool getFfbActive();

	void reset_ffb();
	void start_FFB();
	void stop_FFB();

	void restoreFlash();
	void saveFlash();

private:
	// HID

	uint8_t find_free_effect(uint8_t type);
	void new_effect(FFB_CreateNewEffect_Feature_Data_t* effect);
	void free_effect(uint16_t id);
	void ffb_control(uint8_t cmd);
	void set_effect(FFB_SetEffect_t* effect);
	void set_condition(FFB_SetCondition_Data_t* cond);

	void set_constant_effect(FFB_SetConstantForce_Data_t* effect);
	void set_periodic(FFB_SetPeriodic_Data_t* report);


	void set_filters(FFB_Effect* effect);

	uint32_t hid_out_period = 0; // ms since last out report for measuring update rate

	uint8_t idlespringstregth = 127;
	uint8_t frictionscale = 127;

	uint8_t last_effect_id = 0;
	uint16_t used_effects = 0;
	uint8_t gain = 0xff;
	bool ffb_active = false;
	FFB_BlockLoad_Feature_Data_t blockLoad_report;
	FFB_PIDPool_Feature_Data_t pool_report;

	reportFFB_status_t reportFFBStatus;
	FFB_Effect effects[MAX_EFFECTS];

	uint32_t lastOut = 0;

};

#endif /* HIDFFB_H_ */
