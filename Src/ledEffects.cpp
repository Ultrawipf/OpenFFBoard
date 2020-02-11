/*
 * ledEffects.c
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */
#include "ledEffects.h"
#include "main.h"

uint32_t sysledtick=0;
uint32_t errledtick=0;
uint32_t clipledtick=0;

void pulseSysLed(){
	sysledtick = HAL_GetTick();
	HAL_GPIO_WritePin(LED_SYS_GPIO_Port, LED_SYS_Pin, GPIO_PIN_SET);
}

void pulseErrLed(){
	errledtick = HAL_GetTick();
	HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, GPIO_PIN_SET);
}

void pulseClipLed(){
	clipledtick = HAL_GetTick();
	HAL_GPIO_WritePin(LED_CLIP_GPIO_Port, LED_CLIP_Pin, GPIO_PIN_SET);
}

void updateLeds(){
	if(sysledtick!=0 && HAL_GetTick() > sysledtick+35){
		HAL_GPIO_WritePin(LED_SYS_GPIO_Port, LED_SYS_Pin, GPIO_PIN_RESET);
		sysledtick = 0;
	}
	if(errledtick!=0 && HAL_GetTick() > errledtick+250){
		HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, GPIO_PIN_RESET);
		errledtick = 0;
	}
	if(clipledtick!=0 && HAL_GetTick() > clipledtick+100){
		HAL_GPIO_WritePin(LED_CLIP_GPIO_Port, LED_CLIP_Pin, GPIO_PIN_RESET);
		clipledtick = 0;
	}
}
