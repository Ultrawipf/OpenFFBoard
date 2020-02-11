/*
 * EncoderLocal.cpp
 *
 *  Created on: 02.02.2020
 *      Author: Yannick
 */

#include "EncoderLocal.h"

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
	 htim->CNT = 0;
}

void EncoderLocal::setOffset(int32_t offset){
	this->offset = offset;
}

void EncoderLocal::setPeriod(uint32_t period){
	this->htim->ARR = period;
}

void EncoderLocal::overflowCallback(){
	if(htim->CNT > this->htim->ARR/2){
		pos -= htim->ARR;
	}else{
		pos += htim->ARR;
	}
}
