/*
 * MotorPWM.cpp
 *
 *  Created on: Mar 29, 2020
 *      Author: Yannick
 */


/*
 * Contains motor drivers based on PWM generation
 */
#include <MotorPWM.h>



ClassIdentifier MotorPWM_RC::info = {
		 .name = "PWM RC" ,
		 .id=2
 };
const ClassIdentifier MotorPWM_RC::getInfo(){
	return info;
}

void MotorPWM_RC::turn(int16_t power){
	if(!active)
		return;
	int32_t val = (uint32_t)((power * 1000)/0xffff);
	val = clip(1500-val,1000, 2000);
	setPWM(val,timer,channel,period);
}

MotorPWM_RC::MotorPWM_RC() {
	// Setup PWM timer (timer2!) on CS3 pin
	HAL_GPIO_DeInit(SPI1_SS3_GPIO_Port, SPI1_SS3_Pin);
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = SPI1_SS3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(SPI1_SS3_GPIO_Port, &GPIO_InitStruct);

	pwmInitTimer(timer, channel,period);
}



MotorPWM_RC::~MotorPWM_RC() {
	HAL_TIM_PWM_Stop(timer, channel);
}


void MotorPWM_RC::start(){
	active = true;
}

void MotorPWM_RC::stop(){
	active = false;
	setPWM(1500,timer,channel,period);
}



/*
 * This motor driver generates a pwm signal for H-bridge drivers on the CS pins.
 *
 */

ClassIdentifier MotorPWM_HB::info = {
		 .name = "PWM H-Bridge" ,
		 .id=3
 };
const ClassIdentifier MotorPWM_HB::getInfo(){
	return info;
}

void MotorPWM_HB::turn(int16_t power){
	if(!active)
		return;

	if(power < 0){
		HAL_GPIO_WritePin(leftPort, leftPin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(rightPort, rightPin, GPIO_PIN_RESET);
	}else{
		HAL_GPIO_WritePin(leftPort, leftPin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(rightPort, rightPin, GPIO_PIN_SET);

	}
	int32_t val = (uint32_t)((abs(power) * period)/0xffff);
	setPWM(val,timer,channel,period);
}

MotorPWM_HB::MotorPWM_HB() {
	// Setup PWM timer (timer2!) on CS3 pin
	HAL_GPIO_DeInit(SPI1_SS3_GPIO_Port, SPI1_SS3_Pin);
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = SPI1_SS3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(SPI1_SS3_GPIO_Port, &GPIO_InitStruct);

	pwmInitTimer(timer, channel,period);
}



MotorPWM_HB::~MotorPWM_HB() {
	HAL_TIM_PWM_Stop(timer, channel);
}


void MotorPWM_HB::start(){
	active = true;
}

void MotorPWM_HB::stop(){
	active = false;
	setPWM(0,timer,channel,period);
}



void pwmInitTimer(TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period){

	timer->Init.Prescaler = 95;
	timer->Init.CounterMode = TIM_COUNTERMODE_UP;
	timer->Init.Period = period;
	timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	timer->Instance->CCMR1 |= TIM_CCMR1_OC1PE;
	HAL_TIM_PWM_Init(timer);

	TIM_OC_InitTypeDef oc_init;
	oc_init.OCMode = TIM_OCMODE_PWM1;
	oc_init.OCPolarity = TIM_OCPOLARITY_HIGH;
	oc_init.Pulse = 0;
	HAL_TIM_PWM_ConfigChannel(timer, &oc_init, channel);
	HAL_TIM_PWM_Start(timer, channel);
}

/*
 * Changes the pwm value of the timer
 */
void setPWM(uint16_t value,TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period){
    TIM_OC_InitTypeDef sConfigOC;

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = value;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(timer, &sConfigOC, channel);
    HAL_TIM_PWM_Start(timer, channel);
}
