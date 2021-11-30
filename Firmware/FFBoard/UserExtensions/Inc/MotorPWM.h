/*
 * MotorPWM.h
 *
 *  Created on: Mar 29, 2020
 *      Author: Yannick
 */

#ifndef MOTORPWM_H_
#define MOTORPWM_H_

#include <MotorDriver.h>
#include "cppmain.h"
#include "CommandHandler.h"
#include "TimerHandler.h"
#include "target_constants.h"
#include "PersistentStorage.h"

extern TIM_HandleTypeDef TIM_PWM; // Htim2 is 32 bit

void pwmInitTimer(TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period,uint32_t prescaler);
void setPWM_HAL(uint32_t value,TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period);

enum class ModePWM_DRV : uint8_t {RC_PWM=0,CENTERED_PWM=1,PWM_DIR=2,PWM_DUAL=3};
enum class SpeedPWM_DRV : uint8_t {LOW=0,MID=1,HIGH=2,VERYHIGH=3};


/**
 * Contains motor drivers methods based on PWM generation.
 *
 * Can output different types of PWM signals based on a torque input.
 */

class MotorPWM: public MotorDriver,public CommandHandler,public PersistentStorage{
	enum class MotorPWM_commands : uint32_t {
		mode,freq
	};
public:
	MotorPWM();
	virtual ~MotorPWM();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	static bool isCreatable();

	void turn(int16_t power);
	void stopMotor();
	void startMotor();

	void setPwmSpeed(SpeedPWM_DRV spd);
	SpeedPWM_DRV getPwmSpeed();

	void setMode(ModePWM_DRV mode);
	ModePWM_DRV getMode();

	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	virtual std::string getHelpstring(){return "PWM output motor driver";}

	void setPWM(uint32_t value,uint8_t ccr);

	static bool pwmDriverInUse;

private:
	float tFreq = 1; // Frequency scaling. Timer freq in MHz
	int32_t period = 20000;
	int32_t prescaler = 95;


	SpeedPWM_DRV pwmspeed = SpeedPWM_DRV::LOW;
	ModePWM_DRV mode = ModePWM_DRV::CENTERED_PWM;

	bool active = false;

	// CCR and channels must match!
	const uint32_t channel_1 = TIM_CHANNEL_1;
	const uint32_t channel_2 = TIM_CHANNEL_2;
	const uint32_t channel_3 = TIM_CHANNEL_3;
	const uint32_t channel_4 = TIM_CHANNEL_4;

	const uint8_t ccr_1 = 1;
	const uint8_t ccr_2 = 2;
	const uint8_t ccr_3 = 3;
	const uint8_t ccr_4 = 4;

	TIM_HandleTypeDef* timer = &TIM_PWM;
};


#endif /* MOTORPWM_H_ */
