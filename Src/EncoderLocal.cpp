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

}

EncoderLocal::~EncoderLocal() {
	// TODO Auto-generated destructor stub
}


int32_t EncoderLocal::getPos(){

	return htim->CNT + pos + offset;
}
void EncoderLocal::setPos(int32_t pos){
	this->pos = pos;
	htim->CNT = pos % htim->ARR;
}

void EncoderLocal::setOffset(int32_t offset){
	this->offset = offset;
}

void EncoderLocal::setPeriod(uint32_t period){
	this->htim->ARR = period-1;
}

void EncoderLocal::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == ENCODER_Z_Pin){
		overflowCallback();
	}
}

void EncoderLocal::overflowCallback(){
	if(htim->CNT > this->htim->ARR/2){
		pos -= htim->ARR+1;
	}else{
		pos += htim->ARR+1;
	}
}
