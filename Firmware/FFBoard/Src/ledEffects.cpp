/*
 * ledEffects.c
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */
#include "ledEffects.h"
#include "main.h"

Ledstruct_t sysled{
	0,0,0,LED_SYS_GPIO_Port,LED_SYS_Pin,0
};
Ledstruct_t errled{
	0,0,0,LED_ERR_GPIO_Port,LED_ERR_Pin,0
};
Ledstruct_t clipled{
	0,0,0,LED_CLIP_GPIO_Port,LED_CLIP_Pin,0
};

/**
 * Blinks led x times with period in ms
 * 0 blinks causes led to blink forever.
 * To stop blinking set period and blinks to 0
 */
void blinkLed(Ledstruct* led,uint16_t period,uint16_t blinks){
	// Stop blinking
	if(period == 0 && blinks == 0){
		led->blinks = 0;
		led->period = 0;
		HAL_GPIO_WritePin(led->port, led->pin, led->state ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}else{
		led->blinks = (blinks * 2) -1;
		led->period = period;
		led->tick = HAL_GetTick();
		HAL_GPIO_WritePin(led->port, led->pin, led->state ? GPIO_PIN_RESET : GPIO_PIN_SET);
	}
}

void pulseSysLed(){
	if(sysled.period == 0) // only overwrite with a pulse if not already blinking
		blinkLed(&sysled, 25, 1);
}

void pulseErrLed(){
	if(errled.period == 0)
		blinkLed(&errled, 250, 1);
}

void pulseClipLed(){
	if(clipled.period == 0)
		blinkLed(&clipled, 100, 1);
}


void blinkSysLed(uint16_t period,uint16_t blinks){
	blinkLed(&sysled, period, blinks);
}
void blinkErrLed(uint16_t period,uint16_t blinks){
	blinkLed(&errled, period, blinks);
}
void blinkClipLed(uint16_t period,uint16_t blinks){
	blinkLed(&clipled, period, blinks);
}

void setLed(Ledstruct_t* led,uint8_t on){
	led->state = on;
	HAL_GPIO_WritePin(led->port, led->pin, led->state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void setClipLed(uint8_t on){
	setLed(&clipled,on);
}

void setErrLed(uint8_t on){
	setLed(&errled,on);
}

void setSysLed(uint8_t on){
	setLed(&sysled,on);
}


void updateLed(Ledstruct* led){
	// If led has an effect (period != 0) and time is up do something
	if(led->period != 0 && HAL_GetTick() > led->tick+led->period){
		if(led->blinks == 0){ // No blinks left. turn off
			HAL_GPIO_WritePin(led->port, led->pin, led->state ? GPIO_PIN_SET : GPIO_PIN_RESET);
			led->period = 0;
		}else{
			led->tick = HAL_GetTick();
			HAL_GPIO_TogglePin(led->port, led->pin);
		}
		// If positive decrement blink counter.
		if(led->blinks > 0){
			led->blinks--;
		}
	}
}

/**
 * Must be called periodically to update the state of LEDs
 */
void updateLeds(){
	updateLed(&clipled);
	updateLed(&errled);
	updateLed(&sysled);
}
