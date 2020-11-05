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

extern TIM_HandleTypeDef htim2; // Htim2 is 32 bit

void pwmInitTimer(TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period,uint32_t prescaler);
void setPWM_HAL(uint32_t value,TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period);

enum class ModePWM_DRV : uint8_t {RC_PWM=0,CENTERED_PWM=1,PWM_DIR=2,PWM_DUAL=3};

enum class SpeedPWM_DRV : uint8_t {LOW=0,MID=1,HIGH=2,VERYHIGH=3};


class MotorPWM: public MotorDriver,public CommandHandler{
public:
	MotorPWM();
	virtual ~MotorPWM();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void turn(int16_t power);
	void stop();
	void start();

	void setPwmSpeed(SpeedPWM_DRV spd);
	SpeedPWM_DRV getPwmSpeed();

	void setMode(ModePWM_DRV mode);
	ModePWM_DRV getMode();

	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash

	ParseStatus command(ParsedCommand* cmd,std::string* reply);

	void setPWM(uint32_t value,uint8_t ccr);

private:
	const uint32_t basefreq = 96;
	float tFreq = 1; // Frequency scaling. Timer freq in MHz
	int32_t period = 20000;
	int32_t prescaler = 95;


	SpeedPWM_DRV pwmspeed = SpeedPWM_DRV::LOW;
	ModePWM_DRV mode = ModePWM_DRV::RC_PWM;

	bool active = false;
	const uint32_t channel_1 = TIM_CHANNEL_3; // Motor port CS3 (also update pwm method)
	const uint32_t channel_2 = TIM_CHANNEL_2;
	const uint8_t ccr_1 = 3;
	const uint8_t ccr_2 = 2;

	TIM_HandleTypeDef* timer = &htim2;

	GPIO_TypeDef* pwm1Port=SPI1_SS3_GPIO_Port;
	const uint16_t pwm1Pin=SPI1_SS3_Pin;

	GPIO_TypeDef* pwm2Port=GP4_GPIO_Port;
	const uint16_t pwm2Pin=GP4_Pin;

	GPIO_TypeDef* leftPort=SPI1_SS1_GPIO_Port;
	const uint16_t leftPin=SPI1_SS1_Pin;

	GPIO_TypeDef* rightPort=SPI1_SS2_GPIO_Port;
	const uint16_t rightPin=SPI1_SS2_Pin;

};


#endif /* MOTORPWM_H_ */
