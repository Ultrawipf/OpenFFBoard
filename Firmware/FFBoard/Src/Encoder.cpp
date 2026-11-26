/*
 * Encoder.cpp
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */

#include "Encoder.h"
#include "ClassChooser.h"
#include "EncoderManager.h"

ClassIdentifier Encoder::info ={.name = "None" , .id=CLSID_ENCODER_NONE, .visibility = ClassVisibility::visible};
ClassIdentifier EncoderNone::info ={.name = "None" , .id=CLSID_ENCODER_NONE, .visibility = ClassVisibility::visible};


const ClassIdentifier Encoder::getInfo(){
	return info;
}

Encoder::Encoder() {
	last_update_time = micros();
}

Encoder::~Encoder() {
	EncoderManager::getInstance().deregisterEncoder(this);
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

float Encoder::getSpeed() {
    return pll.getSpeed(); // rot/s
}

float Encoder::getAccel() {
    return pll.getAccel(); // rot/s^2
}

uint32_t Encoder::getScaler() {
	return 1;
}

void Encoder::triggerRead() {
    // Calculate exact microsecond time step (dt) since the last update
    uint32_t now = micros();
    uint32_t dt_us = now - last_update_time;
    if (dt_us == 0) return; // Prevent zero-time step
    float dt = (float)dt_us * 1e-6f; // Convert microseconds to seconds (float)
    last_update_time = now;

    // For synchronous/polling-based encoders (e.g. incremental ABN),
    // getPos_f() reads STM32 register counts instantaneously.
    updateState(getPos_f(), dt);
}

void Encoder::updateState(float new_pos_rot, float dt) {
    // 1. Adapt PLL bandwidth dynamically based on encoder CPR resolution
    uint32_t current_cpr = getCpr();
    if (current_cpr != last_cpr && current_cpr > 0) {
        last_cpr = current_cpr;
        if (current_cpr >= 65536) {
            pll.setTuning(130.0f, 1.0f, 60.0f);
        } else if (current_cpr >= 16384) {
            pll.setTuning(90.0f, 1.0f, 60.0f);
        } else {
            pll.setTuning(60.0f, 1.0f, 50.0f);
        }
    }

    // 2. Update 2nd-order Type-II PLL in continuous rotations
    pll.update(new_pos_rot, dt);

    // 3. Trigger FOC update for any attached external adapters (Cascade ISR)
    EncoderManager::getInstance().updateAdaptersForEncoder(this);
}




