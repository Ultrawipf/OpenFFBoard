/*
 * EncoderLocal.cpp
 *
 *  Created on: 02.02.2020
 *      Author: Yannick
 */

#include "EncoderLocal.h"

ClassIdentifier EncoderLocal::info = {
		 .name = "Local" ,
		 .id=2
 };
const ClassIdentifier EncoderLocal::getInfo(){
	return info;
}

EncoderLocal::EncoderLocal() {
	this->htim = &TIM_ENC;
	setPos(0);
	HAL_TIM_Base_Start_IT(htim);

	this->htim->Instance->CR1 = 1;
}

EncoderLocal::~EncoderLocal() {
	// TODO Auto-generated destructor stub
}


int32_t EncoderLocal::getPos(){
	int32_t timpos = htim->Instance->CNT - 0x7fff;
	return timpos + offset;
}
void EncoderLocal::setPos(int32_t pos){
	this->pos = pos;
	htim->Instance->CNT = pos+0x7fff;
}

void EncoderLocal::setOffset(int32_t offset){
	this->offset = offset;
}

void EncoderLocal::setPeriod(uint32_t period){
	this->htim->Instance->ARR = period-1;
}

void EncoderLocal::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == ENCODER_Z_Pin){
		overflowCallback();
	}
}

void EncoderLocal::timerElapsed(TIM_HandleTypeDef* htim){
	if(htim == this->htim){
		overflowCallback();
	}
}

void EncoderLocal::overflowCallback(){
	if(htim->Instance->CNT > this->htim->Instance->ARR/2){
		pos -= htim->Instance->ARR+1;
	}else{
		pos += htim->Instance->ARR+1;
	}
}
