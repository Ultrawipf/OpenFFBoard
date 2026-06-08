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
	EncoderManager::getInstance().registerEncoder(this);
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
    return kalman.getOmega() / (2.0f * 3.14159265f); // rad/s -> rot/s
}

float Encoder::getAccel() {
    return kalman.getAlpha() / (2.0f * 3.14159265f); // rad/s^2 -> rot/s^2
}

uint32_t Encoder::getScaler() {
	return 1;
}

void Encoder::triggerRead() {
    // Calculate exact microsecond time step (dt) since the last update
    uint32_t now = micros();
    uint32_t dt_us = now - last_update_time;
    if (dt_us == 0) return; // Prevent zero-time prediction step
    float dt = (float)dt_us * 1e-6f; // Convert microseconds to seconds (float)
    last_update_time = now;
    
    // For synchronous/polling-based encoders (e.g. incremental ABN), 
    // getPos_f() reads STM32 register counts instantaneously.
    // Update the Kinematic Kalman filter state with the new position.
    updateState(getPos_f(), dt);
}

void Encoder::updateState(float new_pos_rot, float dt) {
    // 1. Predict state transition using current time step dt
    kalman.predict(dt);

    // 2. Adjust measurement noise covariance R dynamically on CPR transitions.
    // Quantization noise covariance for uniform distribution: R = (step^2) / 12.
    uint32_t current_cpr = getCpr();
    if (current_cpr != last_cpr) {
        last_cpr = current_cpr;
        if (current_cpr > 0) {
            float step = 2.0f * 3.14159265f / (float)current_cpr;
            r_var = (step * step) / 12.0f;
        } else {
            r_var = 0.0001f; // Safe default variance
        }
    }

    // 3. Convert position from rotations to SI radians
    float new_pos_rad = new_pos_rot * 2.0f * 3.14159265f;

    // 4. Correct the Kalman state estimate with the new measurement and noise
    kalman.update(new_pos_rad, r_var);
}




