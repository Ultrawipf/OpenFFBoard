/*
 * Encoder.cpp
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */

#include "Encoder.h"
#include "ClassChooser.h"
#include "EncoderLocal.h"

// 0-63 valid ids
std::vector<class_entry<Encoder>> const Encoder::all_encoders =
	{
		add_class<Encoder, Encoder>(),
#ifdef LOCALENCODER

		add_class<EncoderLocal, Encoder>(),
#endif
};

ClassIdentifier Encoder::info ={.name = "None" , .id=0, .unique = '0', .hidden = false};


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

/*
 * Gets the amount of counts per full rotation of the encoder
 */
uint32_t Encoder::getCpr(){
	return this->cpr;
}


/*
 * Returns the encoder position as raw counts
 */
int32_t Encoder::getPos(){
	return 0;
}

/*
 * Returns a float position in rotations
 */
float Encoder::getPos_f(){
	if(getCpr() == 0){
		return 0.0; // cpr not set.
	}
	return (float)this->getPos() / (float)this->getCpr();
}

/*
 * Change the position of the encoder
 * Can be used to reset the center
 */
void Encoder::setPos(int32_t pos){

}




