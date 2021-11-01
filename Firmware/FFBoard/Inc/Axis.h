/*
 * Axis.h
 *
 *  Created on: 21.01.2021
 *      Author: Yannick / Lidders
 */

#ifndef SRC_AXIS_H_
#define SRC_AXIS_H_
#include <CmdParser.h>
#include <FFBoardMain.h>
#include <MotorPWM.h>
#include "usb_hid_ffb_desc.h"
#include "TMC4671.h"
#include "PersistentStorage.h"
#include "ButtonSource.h"
#include "EncoderLocal.h"
#include "LocalAnalog.h"
#include "AnalogSource.h"

#include "cppmain.h"
#include "HidFFB.h"
#include "ffb_defs.h"
#include "hid_cmd_defs.h"
#include "TimerHandler.h"
#include "ClassChooser.h"
#include "ExtiHandler.h"
#include "EffectsCalculator.h"
#include "FastAvg.h"
//#include "NormalizedAxis.h"


struct Control_t {
	bool emergency = false;
	bool usb_disabled = true;
	bool update_disabled = true;
	bool request_update_disabled = false;
	bool usb_update_flag = false;
	bool update_flag = false;
	bool resetEncoder = false;
};

struct AxisFlashAddrs
{
	uint16_t config = ADR_AXIS1_CONFIG;
	uint16_t maxSpeed = ADR_AXIS1_MAX_SPEED;
	uint16_t maxAccel = ADR_AXIS1_MAX_ACCEL;
	uint16_t endstop = ADR_AXIS1_ENDSTOP;

	uint16_t power = ADR_AXIS1_POWER;
	uint16_t degrees = ADR_AXIS1_DEGREES;
	uint16_t effects1 = ADR_AXIS1_EFFECTS1;
};

struct AxisConfig
{
	uint8_t drvtype = 0;
	uint8_t enctype = 0;
	//bool invert = false;
};
struct metric_t {
	float accel = 0;	// in deg/s²
	float speed = 0;
	float speedInstant = 0; // in deg/s
	int32_t pos = 0;
	float posDegrees = 0;
	int32_t torque = 0; // total of effect + endstop torque
};


struct axis_metric_t {
	metric_t current;
	metric_t previous;
};
class Axis : public PersistentStorage, public CommandHandler,public HidCommandHandler
{
public:
	Axis(char axis, volatile Control_t* control);
	virtual ~Axis();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	// TODO add new commands
	virtual std::string getHelpstring() { return "\nAxis commands: Get: axis.cmd , Set: axis.cmd=var, where axis = x-z e.g. y.power\n"
												 "power,zeroenc,enctype,pos,degrees,esgain,fxratio,invert,drvtype,idlespring,axisdamper\n"
													;}
	void setupTMC4671();

	// Dynamic classes
	void setDrvType(uint8_t drvtype);
	void setEncType(uint8_t enctype);
	uint8_t getDrvType();
	uint8_t getEncType();

	void usbSuspend(); // Called on usb disconnect and suspend
	void usbResume();  // Called on usb resume

	void saveFlash() override;
	void restoreFlash() override;

	void prepareForUpdate();  // called before the effects are calculated
	void updateDriveTorque(); //int32_t effectTorque);
	void emergencyStop();

	void setPos(uint16_t val);

	bool getFfbActive();

	int32_t scaleEncValue(float angle, uint16_t degrees);
	float 	getEncAngle(Encoder *enc);
	float	getNormalizedSpeedScaler(uint16_t maxSpeedRpm, uint16_t degrees);
	float	getNormalizedAccelScaler(uint16_t maxAccelRpm, uint16_t degrees);
	float	getSpeedFromNormalized(uint16_t speedNormalized, uint16_t degrees);
	float	getAccelFromNormalized(uint16_t accelNormalized, uint16_t degrees);


	void setPower(uint16_t power);

	ParseStatus command(ParsedCommand* cmd,std::string* reply) override;
	void processHidCommand(HID_Custom_Data_t *data) override;
	ClassChooser<MotorDriver> drv_chooser;
	ClassChooser<Encoder> enc_chooser;

	int32_t getLastScaledEnc();
	void resetMetrics(float new_pos);
	void updateMetrics(float new_pos);
	int32_t updateIdleSpringForce();
	void setIdleSpringStrength(uint8_t spring);
	void setDamperStrength(uint8_t damper);
	void calculateAxisEffects(bool ffb_on);
	int32_t getTorque(); // current torque scaled as a 32 bit signed value
	int16_t updateEndstop();

	metric_t* getMetrics();
	float 	 getSpeedScalerNormalized();
	float	 getAccelScalerNormalized();
	uint16_t getSpeedRPMFromEncValue(uint16_t speedNormalized, uint16_t degrees);
	float 	 getAccelFromEncValue(float accelNormalized, uint16_t degrees);


	void setEffectTorque(int32_t torque);
	bool updateTorque(int32_t* totalTorque);


private:
	AxisFlashAddrs flashAddrs;
	volatile Control_t* control;

	//TIM_HandleTypeDef *timer_update;
	AxisConfig conf;

	std::unique_ptr<MotorDriver> drv = std::make_unique<MotorDriver>(); // dummy
	std::shared_ptr<Encoder> enc = nullptr;

	bool tmcFeedForward = false; // Experimental

	static AxisConfig decodeConfFromInt(uint16_t val);
	static uint16_t encodeConfToInt(AxisConfig conf);

	const TMC4671PIDConf tmcpids = TMC4671PIDConf({.fluxI = 400,
											 .fluxP = 400,
											 .torqueI = 400,
											 .torqueP = 300,
											 .velocityI = 0,
											 .velocityP = 128,
											 .positionI = 0,
											 .positionP = 64,
											 .sequentialPI = true});
	TMC4671Limits tmclimits = TMC4671Limits({.pid_torque_flux_ddt = 32767,
											 .pid_uq_ud = 30000,
											 .pid_torque_flux = 30000,
											 .pid_acc_lim = 2147483647,
											 .pid_vel_lim = 2147483647,
											 .pid_pos_low = -2147483647,
											 .pid_pos_high = 2147483647});

	// Lowpass 1KHZ
	const TMC4671Biquad fluxbq = TMC4671Biquad({.a1 = -134913,
										  .a2 = 536735999,
										  .b0 = 67457,
										  .b1 = 134913,
										  .b2 = 67457,
										  .enable = true});

	int32_t torqueFFgain = 50000;
	int32_t torqueFFconst = 0;
	int32_t velocityFFgain = 30000;
	int32_t velocityFFconst = 0;

	uint16_t degreesOfRotation = 900;					// How many degrees of range for the full gamepad range
	uint16_t lastdegreesOfRotation = degreesOfRotation; // Used to store the previous value
	uint16_t nextDegreesOfRotation = degreesOfRotation; // Buffer when changing range

	// Check if all of these are needed:
	float speedScalerNormalized = 1;
	float accelScalerNormalized = 1;
	uint16_t maxHumanSpeedRpm 	= 200;
	float	 maxHumanAccelRpmm 	= 30.0;

	FastAvg<10> speed_avg;
	FastAvg<10> accel_avg;

	bool	 calibrationInProgress;
	uint16_t calibMaxSpeedNormalized;
	float	 calibMaxAccelNormalized;

	void setDegrees(uint16_t degrees);

	uint16_t getPower();
	float getTorqueScaler();
	bool isInverted();
	char axis;


	// Merge normalized
	axis_metric_t metric;
	int32_t effectTorque = 0;
	int32_t axisEffectTorque = 0;
	uint8_t fx_ratio_i = 204; // Reduce effects to a certain ratio of the total power to have a margin for the endstop
	uint16_t power = 2000;
	float torqueScaler = 0; // power * fx_ratio as a ratio between 0 & 1
	bool invertAxis = false;
	uint8_t endstop_gain = 128; // Sets how much extra torque per count above endstop is added. High = stiff endstop. Low = softer

	uint8_t idlespringstrength = 127;
	int16_t idlespringclip = 0;
	float idlespringscale = 0;
	bool idle_center = false;

	float damper_f = 25 , damper_q = 0.2;
	const float filter_f = 500; // 1khz/2
	const int32_t damperClip = 10000;
	uint8_t damperIntensity = 0;
	Biquad damperFilter = Biquad(BiquadType::lowpass, damper_f/filter_f, damper_q, 0.0);

	void setFxRatio(uint8_t val);
	void updateTorqueScaler();
};

#endif /* SRC_AXIS_H_ */
