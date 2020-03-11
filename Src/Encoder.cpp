/*
 * Encoder.cpp
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */

#include "Encoder.h"

ClassIdentifier Encoder::info ={.name = "None" , .id=0, .hidden = true};

const ClassIdentifier Encoder::getInfo(){
	return info;
}

Encoder::Encoder() {
	// TODO Auto-generated constructor stub

}

Encoder::~Encoder() {
	// TODO Auto-generated destructor stub
}

uint32_t Encoder::getPpr(){
	return this->ppr;
}

uint32_t Encoder::getPosCpr(){
	return this->ppr;
}

void Encoder::setPpr(uint32_t ppr){
	this->ppr = ppr;
}
int32_t Encoder::getPos(){
	return 0;
}
void Encoder::setPos(int32_t pos){

}
