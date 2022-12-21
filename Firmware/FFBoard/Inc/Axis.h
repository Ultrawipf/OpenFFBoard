/*
 * Axis.h
 *
 *  Created on: 21.01.2021
 *      Author: Yannick / Lidders
 */

#ifndef SRC_AXIS_H_
#define SRC_AXIS_H_
#include <FFBoardMain.h>
#include <MotorPWM.h>
#include "usb_hid_ffb_desc.h"
#include "TMC4671.h"
#include "PersistentStorage.h"
#include "ButtonSource.h"
#include "EncoderLocal.h"
#include "LocalAnalog.h"
#include "AnalogSource.h"

#include "HidFFB.h"
#include "ffb_defs.h"
#include "TimerHandler.h"
#include "ClassChooser.h"
#include "ExtiHandler.h"
#include "EffectsCalculator.h"
#include "FastAvg.h"

#define INTERNAL_AXIS_DAMPER_SCALER 0.7


struct Control_t {
	bool emergency = false;
	bool usb_disabled = true;
	bool update_disabled = true;
	bool request_update_disabled = false;
//	bool usb_update_flag = false;
//	bool update_flag = false;
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
	uint16_t encoderRatio = ADR_AXIS1_ENC_RATIO;

	uint16_t speedAccelFilter = ADR_AXIS1_SPEEDACCEL_FILTER;
};

struct AxisConfig
{
	uint8_t drvtype = 0;
	uint8_t enctype = 0;
	//bool invert = false;
};
struct metric_t {
	float accel = 0;	// in deg/s²
	float speed = 0; // in deg/s
	int32_t pos = 0;
	float posDegrees = 0;
	int32_t torque = 0; // total of effect + endstop torque
};


struct axis_metric_t {
	metric_t current;
	metric_t previous;
};

struct GearRatio_t{
	uint8_t denominator = 0;
	uint8_t numerator = 0;
	float gearRatio = 1.0;
};


enum class Axis_commands : uint32_t{
	power=0x00,degrees=0x01,esgain,zeroenc,invert,idlespring,axisdamper,enctype,drvtype,
	pos,maxspeed,maxtorquerate,fxratio,curtorque,curpos,curspd,curaccel,reductionScaler,
	filterSpeed, filterAccel, filterProfileId,cpr
};

class Axis : public PersistentStorage, public CommandHandler, public ErrorHandler
{
public:
	Axis(char axis, volatile Control_t* control);
	virtual ~Axis();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	const ClassType getClassType() override {return ClassType::Axis;};

	virtual std::string getHelpstring() { return "FFB axis"	;}
#ifdef TMC4671DRIVER
	void setupTMC4671();
#endif
	// Dynamic classes
	void setDrvType(uint8_t drvtype);
	void setEncType(uint8_t enctype);
	uint8_t getDrvType();
	uint8_t getEncType();

	Encoder* getEncoder();
	MotorDriver* getDriver();

	void usbSuspend(); // Called on usb disconnect and suspend
	void usbResume();  // Called on usb resume

	void saveFlash() override;
	void restoreFlash() override;

	void prepareForUpdate();  // called before the effects are calculated
	void updateDriveTorque(); //int32_t effectTorque);
	void emergencyStop(bool reset);

	void setPos(uint16_t val);
	void zeroPos();

	bool getFfbActive();

	int32_t scaleEncValue(float angle, uint16_t degrees);
	float 	getEncAngle(Encoder *enc);


	void setPower(uint16_t power);


	void errorCallback(const Error &error, bool cleared) override;

	//ParseStatus command(ParsedCommand_old* cmd,std::string* reply) override;
	void registerCommands();
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);

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

	void setEffectTorque(int32_t torque);
	bool updateTorque(int32_t* totalTorque);

	void setGearRatio(uint8_t numerator,uint8_t denominator);

	static const std::vector<class_entry<MotorDriver>> axis1_drivers;
	static const std::vector<class_entry<MotorDriver>> axis2_drivers;

private:
	// Axis damper is lower than default scale of HID Damper
	const float AXIS_DAMPER_RATIO = INTERNAL_SCALER_DAMPER * INTERNAL_AXIS_DAMPER_SCALER / 255.0;

	AxisFlashAddrs flashAddrs;
	volatile Control_t* control;

	//TIM_HandleTypeDef *timer_update;
	AxisConfig conf;

	std::unique_ptr<MotorDriver> drv = std::make_unique<MotorDriver>(); // dummy
	std::shared_ptr<Encoder> enc = nullptr;

	bool outOfBounds = false;

	static AxisConfig decodeConfFromInt(uint16_t val);
	static uint16_t encodeConfToInt(AxisConfig conf);

	const Error outOfBoundsError = Error(ErrorCode::axisOutOfRange,ErrorType::warning,"Axis out of bounds");

#ifdef TMC4671DRIVER
	TMC4671Limits tmclimits = TMC4671Limits({.pid_torque_flux_ddt = 32767,
											 .pid_uq_ud = 30000,
											 .pid_torque_flux = 30000,
											 .pid_acc_lim = 2147483647,
											 .pid_vel_lim = 2147483647,
											 .pid_pos_low = -2147483647,
											 .pid_pos_high = 2147483647});

	// Lowpass 500Hz Q 0.7 @ 25khz
	const TMC4671Biquad_t tmcbq_500hz_07q_25k = TMC4671Biquad_t(
			{ .a1 = 979476766,
			  .a2 = -450370144,
			  .b0 = 1941073,
			  .b1 = 3882145,
			  .b2 = 1941073});

	// Lowpass 1000Hz Q 0.7 @ 25khz
	const TMC4671Biquad_t tmcbq_1000hz_07q_25k = TMC4671Biquad_t(
			{ .a1 = 886773302,
			  .a2 = -378358476,
			  .b0 = 7114021,
			  .b1 = 14228043,
			  .b2 = 7114021});


#endif
	float encoderOffset = 0; // Offset for absolute encoders
	uint16_t degreesOfRotation = 900;					// How many degrees of range for the full gamepad range
	uint16_t lastdegreesOfRotation = degreesOfRotation; // Used to store the previous value
	uint16_t nextDegreesOfRotation = degreesOfRotation; // Buffer when changing range

	// Limiters
	uint16_t maxSpeedDegS  = 0; // Set to non zero to enable. example 1000. 8b * 10?
	//float	 maxAccelDegSS = 0;
	uint32_t maxTorqueRateMS = 0; // 8b * 128?

	float spdlimitreducerI = 0;
	//float acclimitreducerI = 0;
	//const uint8_t accelFactor = 10.0; // Conversion factor between internal and external acc limit

	void setDegrees(uint16_t degrees);

	uint16_t getPower();
	float getTorqueScaler();
	bool isInverted();
	char axis;


	// Merge normalized
	axis_metric_t metric;
	float _lastSpeed = 0;
	int32_t effectTorque = 0;
	int32_t axisEffectTorque = 0;
	uint8_t fx_ratio_i = 204; // Reduce effects to a certain ratio of the total power to have a margin for the endstop. 80% = 204
	uint16_t power = 5000;
	float torqueScaler = 0; // power * fx_ratio as a ratio between 0 & 1
	float effect_margin_scaler = 0;
	bool invertAxis = false;
	uint8_t endstopStrength = 127; // Sets how much extra torque per count above endstop is added. High = stiff endstop. Low = softer
	const float endstopGain = 25; // Overall max endstop intensity


	uint8_t idlespringstrength = 127;
	int16_t idlespringclip = 0;
	float idlespringscale = 0;
	bool idle_center = false;

	// TODO tune these and check if it is really stable and beneficial to the FFB. index 4 placeholder
	const biquad_constant_t filterSpeedCst[4] = {{ 30, 55 }, { 60, 55 }, { 120, 55 }, {120, 55}};
	const biquad_constant_t filterAccelCst[4] = {{ 40, 30 }, { 55, 30 }, { 70, 30 }, {120, 55}};
	const biquad_constant_t filterDamperCst = {60, 55};
	uint8_t filterProfileId = 1; // Default medium (1) as this is the most common encoder resolution and users can go lower or higher if required.
	const float filter_f = 1000; // 1khz
	const int32_t damperClip = 20000;
	uint8_t damperIntensity = 30;
	FastAvg<float,8> spdlimiterAvg;

	Biquad speedFilter = Biquad(BiquadType::lowpass, filterSpeedCst[filterProfileId].freq/filter_f, filterSpeedCst[filterProfileId].q/100.0, 0.0);
	Biquad accelFilter = Biquad(BiquadType::lowpass, filterAccelCst[filterProfileId].freq/filter_f, filterAccelCst[filterProfileId].q/100.0, 0.0);
	Biquad damperFilter = Biquad(BiquadType::lowpass, filterDamperCst.freq/filter_f, filterDamperCst.q / 100.0, 0.0); // enable on class constructor

	void setFxRatio(uint8_t val);
	void updateTorqueScaler();


	GearRatio_t gearRatio;

};

#endif /* SRC_AXIS_H_ */
