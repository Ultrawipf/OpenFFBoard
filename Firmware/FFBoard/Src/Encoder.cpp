/*
 * Encoder.cpp
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */

#include "Encoder.h"

ClassIdentifier Encoder::info ={.name = "None" , .id=0, .hidden = false};


const ClassIdentifier Encoder::getInfo(){
	return info;
}

Encoder::Encoder() {

}

Encoder::~Encoder() {

}

EncoderType Encoder::getType(){
	return EncoderType::NONE;
}

uint32_t Encoder::getCpr(){
	return this->cpr;
}


void Encoder::setCpr(uint32_t cpr){
	this->cpr = cpr;
}
int32_t Encoder::getPos(){
	return 0;
}
void Encoder::setPos(int32_t pos){

}
