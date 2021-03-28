/*
 * NormalizedAxis.h
 *
 *  Created on: 21.01.2021
 *      Author: Lidgard
 */

#ifndef SRC_NORMALIZEDAXIS_H_
#define SRC_NORMALIZEDAXIS_H_

#include <CmdParser.h>
#include <FFBoardMain.h>
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include "HidCommandHandler.h"
#include "cppmain.h"
#include "HidFFB.h"
#include "ffb_defs.h"
#include "hid_cmd_defs.h"
#include "TimerHandler.h"
#include "ClassChooser.h"
//#include "EffectsCalculator.h"

//#define NORMALIZED_AXIS_CMDS	esgain,fxratio,invert

struct NormalizedAxisFlashAddrs_t {
	uint16_t endstop = ADR_AXIS1_ENDSTOP;
	uint16_t power = ADR_AXIS1_POWER;
	uint16_t degrees = ADR_AXIS1_DEGREES;
};

struct metric_t {
	int32_t accel = 0;
	int32_t speed = 0;
	int32_t pos = 0;
	int32_t torque = 0; // total of effect + endstop torque
};


struct axis_metric_t {

	metric_t current;
	metric_t previous;
};


class NormalizedAxis: public PersistentStorage, public CommandHandler,public HidCommandHandler {
public:
	NormalizedAxis(char axis);
	virtual ~NormalizedAxis();

	//ClassIdentifier getInfo() override;
	static ClassIdentifier info;
	const ClassIdentifier getInfo();

    void saveFlash();
	void restoreFlash();

	int32_t getLastScaledEnc();
	void resetMetrics(int32_t new_pos);
	void updateMetrics(int32_t new_pos);
	void updateIdleSpringForce(float idlespringscale, int16_t idlespringclip);
	int32_t getTorque(); // current torque scaled as a 32 bit signed value
	int16_t updateEndstop();
	metric_t* getMetrics();

	
	void setEffectTorque(int32_t torque);
	bool updateTorque(int32_t* totalTorque);
	
	virtual ParseStatus command(ParsedCommand* cmd,std::string* reply);
    virtual void processHidCommand(HID_Custom_Data_t* data);


protected:
	uint16_t degreesOfRotation = 900;					// How many degrees of range for the full gamepad range
	uint16_t nextDegreesOfRotation = degreesOfRotation; // Buffer when changing range

	virtual void setPower(uint16_t power);
	uint16_t getPower();
	float getTorqueScaler();
	bool isInverted();
	char axis;

private:
	axis_metric_t metric;
	int32_t effectTorque = 0;
	uint8_t fx_ratio_i = 204; // Reduce effects to a certain ratio of the total power to have a margin for the endstop
	uint16_t power = 2000;
	float torqueScaler; // power * fx_ratio as a ratio between 0 & 1
	bool invertAxis = false;
	uint8_t endstop_gain = 128; // Sets how much extra torque per count above endstop is added. High = stiff endstop. Low = softer

    NormalizedAxisFlashAddrs_t flashAddrs;
   	void setFxRatio(uint8_t val);
	void updateTorqueScaler();
	
};

#endif /* SRC_NORMALIZEDAXIS_H_ */
