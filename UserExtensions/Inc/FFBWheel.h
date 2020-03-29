/*
 * FFBWheel.h
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#ifndef SRC_FFBWHEEL_H_
#define SRC_FFBWHEEL_H_
#include <FFBoardMain.h>
#include <MotorPWM.h>
#include "TMC4671.h"
#include "flash_helpers.h"
#include "cmdparser.h"

#include "ButtonSource.h"
#include "LocalButtons.h"
#include "SPIButtons.h"
#include "ShifterG29.h"

#include "EncoderLocal.h"

#include "cppmain.h"
#include "HidFFB.h"
#include "AdcHandler.h"
#include "TimerHandler.h"
#include "ClassChooser.h"

enum class EncoderType : uint8_t{
	ABN_LOCAL = 0,TMC=1,NONE
};

//enum class MotorDriverType : uint8_t{
//	TMC4671_type=TMC4671::info.id,NONE // Only tmc implemented
//};



struct FFBWheelConfig{
	uint8_t drvtype = 0;
	uint8_t enctype = 0;
	uint8_t axes = 1;
};

enum class AnalogOffset : uint8_t{
	FULL=0,LOWER=1,UPPER=2,NONE
};

struct FFBWheelAnalogConfig{
	uint8_t analogmask = 0xff;
	AnalogOffset offsetmode;

};


class FFBWheel: public FFBoardMain, UsbHidHandler, AdcHandler, TimerHandler, PersistentStorage{
public:
	FFBWheel();
	virtual ~FFBWheel();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void setupTMC4671();
	void setupTMC4671_enc(PhiE enctype);
	bool executeUserCommand(ParsedCommand* cmd,std::string* reply);

	void setDrvType(uint8_t drvtype);
	void setEncType(uint8_t enctype);
	void setBtnTypes(uint16_t btntypes);
	void addBtnType(uint16_t id);
	void clearBtnTypes();
	ButtonSource* getBtnSrc(uint16_t id);

	void SOF();
	void usbInit(); // initialize a composite usb device

	void saveFlash();
	void restoreFlash();

	void update();

	static FFBWheelConfig decodeConfFromInt(uint16_t val);
	static uint16_t encodeConfToInt(FFBWheelConfig conf);
	static FFBWheelAnalogConfig decodeAnalogConfFromInt(uint16_t val);
	static uint16_t encodeAnalogConfToInt(FFBWheelAnalogConfig conf);


	void adcUpd(volatile uint32_t* ADC_BUF);
	void timerElapsed(TIM_HandleTypeDef* htim);

	bool usb_update_flag = false;
	bool update_flag = false;

	uint16_t degreesOfRotation = 900; // TODO save in flash
	int32_t getEncValue(Encoder* enc,uint16_t degrees);

	uint16_t power = 2000;

	float endstop_gain = 20;

private:
	void send_report();
	int16_t updateEndstop();

	HidFFB* ffb;
	TIM_HandleTypeDef* timer_update;
	int32_t torque = 0; // last torque
	int32_t endstopTorque = 0; // last torque
	FFBWheelConfig conf;

	MotorDriver* drv = nullptr;
	Encoder* enc = nullptr;

	std::vector<ButtonSource*> btns;
	FFBWheelAnalogConfig aconf;
	volatile uint16_t adc_buf[ADC_PINS];
	reportHID_t reportHID;

	int32_t lastScaledEnc = 0;
	int32_t scaledEnc = 0;
	int32_t speed = 0;
	bool tmcFeedForward = true;
	uint16_t btnsources = 1; // Default ID1 = local buttons


//	TMC4671PIDConf tmcpids = TMC4671PIDConf({
//
//		.fluxI		= 2,
//		.fluxP		= 3000,
//		.torqueI	= 1500,
//		.torqueP	= 600,
//		.velocityI	= 0,
//		.velocityP	= 128,
//		.positionI	= 0,
//		.positionP	= 64
//	});
	TMC4671PIDConf tmcpids = TMC4671PIDConf({
		.fluxI		= 20,
		.fluxP		= 3000,
		.torqueI	= 2000,
		.torqueP	= 650,
		.velocityI	= 0,
		.velocityP	= 128,
		.positionI	= 0,
		.positionP	= 64
	});
	TMC4671Limits tmclimits = TMC4671Limits({
		.pid_torque_flux_ddt	= 10000,
		.pid_uq_ud				= 24575,
		.pid_torque_flux		= 32767,
		.pid_acc_lim			= 2147483647,
		.pid_vel_lim			= 2147483647,
		.pid_pos_low			= -2147483647,
		.pid_pos_high			= 2147483647
	});

	// Lowpass 1KHZ
	TMC4671Biquad fluxbq = TMC4671Biquad({
		.a1 = -134913,
		.a2 = 536735999,
		.b0 = 67457,
		.b1 = 134913,
		.b2 = 67457,
		.enable = true
	});
//	TMC4671Biquad fluxbq = TMC4671Biquad({
//			.a1 = -269793,
//			.a2 = 536601119,
//			.b0 = 134896,
//			.b1 = 269793,
//			.b2 = 134896,
//			.enable = true
//		});

	int32_t torqueFFgain = 50000;
	int32_t torqueFFconst = 0;
	int32_t velocityFFgain = 30000;
	int32_t velocityFFconst = 0;

	ClassChooser<ButtonSource> btn_chooser;
	ClassChooser<MotorDriver> drv_chooser;
	ClassChooser<Encoder> enc_chooser;
};

#endif /* SRC_FFBWHEEL_H_ */
