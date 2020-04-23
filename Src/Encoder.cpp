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
	// TODO Auto-generated constructor stub

}

Encoder::~Encoder() {
	// TODO Auto-generated destructor stub
}

EncoderType Encoder::getType(){
	return EncoderType::NONE;
}

uint32_t Encoder::getCpr(){
	return this->ppr;
}


void Encoder::setCpr(uint32_t ppr){
	this->ppr = ppr;
}
int32_t Encoder::getPos(){
	return 0;
}
void Encoder::setPos(int32_t pos){

}
