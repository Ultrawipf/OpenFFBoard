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
#include "NormalizedAxis.h"


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
};

struct AxisConfig
{
	uint8_t drvtype = 0;
	uint8_t enctype = 0;
	bool invert = false;
};

class Axis : public NormalizedAxis
{
public:
	Axis(char axis, volatile Control_t* control);
	virtual ~Axis();

	static ClassIdentifier info;


	virtual std::string getHelpstring() { return "\nAxis commands: Get: axis.cmd , Set: axis.cmd=var, where axis = x-z e.g. y.power\n"
//												 "power,zeroenc,enctype,cpr,pos,degrees,esgain,fxratio,idlespring,spring.friction,damper,inertia,invert,drvtype,tmc.\n"; }
												 "power,zeroenc,enctype,pos,degrees,esgain,fxratio,invert,drvtype,idlespring,axisdamper\n"; }
	void setupTMC4671();
	void setupTMC4671_enc(PhiE enctype);

	void buildReply(std::string *reply, std::string r);

	// Dynamic classes
	void setDrvType(uint8_t drvtype);
	void setEncType(uint8_t enctype);
	uint8_t getDrvType();
	uint8_t getEncType();

	void usbSuspend(); // Called on usb disconnect and suspend
	void usbResume();  // Called on usb resume

	void saveFlash();
	void restoreFlash();

	void prepareForUpdate();  // called before the effects are calculated
	void updateDriveTorque(); //int32_t effectTorque);
	void emergencyStop();

	bool hasEnc();
	void zeroEnc();

	void setPos(uint16_t val);

	bool getFfbActive();

	int32_t getEncValue(Encoder *enc, uint16_t degrees);

	void setPower(uint16_t power) override;
	int16_t updateEndstop();

	ParseStatus command(ParsedCommand* cmd,std::string* reply) override;
	void processHidCommand(HID_Custom_Data_t *data) override;
	ClassChooser<MotorDriver> drv_chooser;
	ClassChooser<Encoder> enc_chooser;

private:
	AxisFlashAddrs flashAddrs;
	volatile Control_t* control;

	void send_report();

	TIM_HandleTypeDef *timer_update;
	AxisConfig conf;

	MotorDriver *drv = nullptr;
	Encoder *enc = nullptr;

	bool tmcFeedForward = false; // Experimental

	void setupTMC4671ForAxis(char axis);
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
};

#endif /* SRC_AXIS_H_ */
