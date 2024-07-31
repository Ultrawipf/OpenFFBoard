/*
 * ledEffects.h
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#ifndef LEDEFFECTS_H_
#define LEDEFFECTS_H_
#include "main.h"
typedef struct Ledstruct{
	uint32_t tick;
	uint16_t period;
	int32_t blinks;
	GPIO_TypeDef* port;
	uint16_t pin;
	uint8_t state;
} Ledstruct_t;


void blinkLed(Ledstruct_t* led,uint16_t period,uint16_t blinks);
void pulseSysLed();
void pulseErrLed();
void pulseClipLed();

void blinkSysLed(uint16_t period,uint16_t blinks);
void blinkErrLed(uint16_t period,uint16_t blinks);
void blinkClipLed(uint16_t period,uint16_t blinks);

void updateLed(Ledstruct_t* led);
void updateLeds();

void setLed(Ledstruct_t* led,uint8_t on);
void setClipLed(uint8_t on);
void setErrLed(uint8_t on);
void setSysLed(uint8_t on);


#endif /* LEDEFFECTS_H_ */
