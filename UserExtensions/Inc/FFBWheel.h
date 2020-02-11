/*
 * FFBWheel.h
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#ifndef SRC_FFBWHEEL_H_
#define SRC_FFBWHEEL_H_
#include <FFBoardMain.h>
#include "TMC4671.h"
#include "flash_helpers.h"
#include "ButtonSource.h"
#include "LocalButtons.h"
#include "cppmain.h"
#include "ffb_defs.h"

enum class EncoderType : uint8_t{
	ABN_LOCAL = 0,ABN_TMC=1,HALL_TMC=2,NONE
};

enum class MotorDriverType : uint8_t{
	TMC4671_type=0,PPM_type=1,NONE // Only tmc implemented
};

enum class ButtonSourceType : uint8_t{
	LOCAL=0,SPI_TM=1,I2C=2,NONE // Only local implemented
};

struct FFBWheelConfig{
	MotorDriverType drvtype=MotorDriverType::TMC4671_type;
	EncoderType enctype=EncoderType::ABN_TMC;
	ButtonSourceType btcsrctype=ButtonSourceType::LOCAL;
};

enum class AnalogOffset : uint8_t{
	FULL=0,LOWER=1,UPPER=2,NONE
};

struct FFBWheelAnalogConfig{
	uint8_t analogmask = 0xff;
	AnalogOffset offsetmode;

};

class FFBWheel: public FFBoardMain  {
public:
	FFBWheel();
	virtual ~FFBWheel();

	static FFBoardMainIdentifier info;
	const FFBoardMainIdentifier getInfo();

	void setupTMC4671();
	void setupTMC4671_enc(EncoderType enctype);
	bool executeUserCommand(ParsedCommand* cmd,std::string* reply);

	void setDrvType(MotorDriverType drvtype);
	void setEncType(EncoderType enctype);
	void setBtnType(ButtonSourceType btntype);

	void SOF();
	void usbInit(); // initialize a composite usb device

	void saveFlash();

	void update();

	static FFBWheelConfig decodeConfFromInt(uint16_t val);
	static uint16_t encodeConfToInt(FFBWheelConfig conf);
	static FFBWheelAnalogConfig decodeAnalogConfFromInt(uint16_t val);
	static uint16_t encodeAnalogConfToInt(FFBWheelAnalogConfig conf);

	void hidOut(uint8_t* report);
	void hidGet(uint8_t id,uint16_t len,uint8_t** return_buf);
	int16_t calculateEffects();
	int16_t updateEndstop();

	void adcUpd(volatile uint32_t* ADC_BUF);
	void timerElapsed(TIM_HandleTypeDef* htim);

	bool usb_update_flag = false;
	bool update_flag = false;

	uint16_t power = 0xffff;
	int32_t getEncValue();

	uint16_t degreesOfRotation = 900; // TODO save in flash

private:
	TIM_HandleTypeDef* timer_update;
	int16_t torque = 0; // last torque
	FFBWheelConfig conf;
	MotorDriver* drv;
	Encoder* enc;
	ButtonSource* btn;
	uint16_t buttonMask = 0xffff;
	FFBWheelAnalogConfig aconf;
	volatile uint16_t adc_buf[ADC_PINS];

	int32_t lastRawEnc = 0;

	// HID
	void send_report();
	uint8_t find_free_effect(uint8_t type);
	void new_effect(FFB_CreateNewEffect_Feature_Data_t* effect);
	void ffb_control(uint8_t cmd);
	void reset_ffb();
	void set_effect(FFB_SetEffect_t* effect);
	void set_condition(FFB_SetCondition_Data_t* cond);

	void set_constant_effect(FFB_SetConstantForce_Data_t* effect);
	void set_periodic(FFB_SetPeriodic_Data_t* report);
	void start_FFB();
	void stop_FFB();

	uint8_t report_counter = 0;
	uint16_t report_counter_hid = 0;
	uint8_t last_effect_id = 0;
	uint16_t used_effects = 0;
	uint8_t gain = 0xff;
	bool ffb_active = false;
	FFB_BlockLoad_Feature_Data_t blockLoad_report;
	FFB_PIDPool_Feature_Data_t pool_report;

	reportFFB_status_t reportFFBStatus;
	FFB_Effect effects[MAX_EFFECTS];
	reportHID_t reportHID;

};

#endif /* SRC_FFBWHEEL_H_ */
