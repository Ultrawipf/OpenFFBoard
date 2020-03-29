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

extern TIM_HandleTypeDef htim2;

void pwmInitTimer(TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period);
void setPWM(uint16_t value,TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period);

class MotorPWM_HB: public MotorDriver {
public:
	MotorPWM_HB();
	virtual ~MotorPWM_HB();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void turn(int16_t power);
	void stop();
	void start();

private:
	const uint32_t period = 8192;
	bool active = false;
	const uint32_t channel = TIM_CHANNEL_3; // Motor port CS3

	GPIO_TypeDef* leftPort=SPI1_SS1_GPIO_Port;
	const uint16_t leftPin=SPI1_SS1_Pin;

	GPIO_TypeDef* rightPort=SPI1_SS2_GPIO_Port;
	const uint16_t rightPin=SPI1_SS2_Pin;

	TIM_HandleTypeDef* timer = &htim2;
};

/*
 * Generates a classic RC 20ms 1000-2000µs signal
 */
class MotorPWM_RC: public MotorDriver {
public:
	MotorPWM_RC();
	virtual ~MotorPWM_RC();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	void turn(int16_t power);
	void stop();
	void start();

private:
	const uint32_t period = 20000;
	bool active = false;
	const uint32_t channel = TIM_CHANNEL_3; // Motor port CS3

	TIM_HandleTypeDef* timer = &htim2;
};
#endif /* MOTORPWM_H_ */
