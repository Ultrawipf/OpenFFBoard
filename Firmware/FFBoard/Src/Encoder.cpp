/*
 * Encoder.cpp
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */

#include "Encoder.h"
#include "ClassChooser.h"

ClassIdentifier Encoder::info ={.name = "None" , .id=CLSID_ENCODER_NONE, .visibility = ClassVisibility::visible};


const ClassIdentifier Encoder::getInfo(){
	return info;
}

Encoder::Encoder() {

}

Encoder::~Encoder() {

}

/**
 * Returns the type of the encoder. Must override this and NOT return NONE in other classes
 */
EncoderType Encoder::getEncoderType(){
	return EncoderType::NONE;
}

/**
 * Gets the amount of counts per full rotation of the encoder
 */
uint32_t Encoder::getCpr(){
	return this->cpr;
}


/**
 * Returns the encoder position as raw counts
 */
int32_t Encoder::getPos(){
	return 0;
}

float Encoder::getPosAbs_f(){
	if(getCpr() == 0){
		return 0.0; // cpr not set.
	}
	return (float)this->getPosAbs() / (float)this->getCpr();
}

/**
 * Returns absolute positions without offsets for absolute encoders.
 * Otherwise it returns getPos
 */
int32_t Encoder::getPosAbs(){
	return getPos();
}


/**
 * Returns a float position in rotations
 */
float Encoder::getPos_f(){
	if(getCpr() == 0){
		return 0.0; // cpr not set.
	}
	return (float)this->getPos() / (float)this->getCpr();
}

/**
 * Change the position of the encoder
 * Can be used to reset the center
 */
void Encoder::setPos(int32_t pos){

}




