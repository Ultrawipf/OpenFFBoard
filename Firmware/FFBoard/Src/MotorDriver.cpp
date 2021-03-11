/*
 * MotorDriver.cpp
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#include "MotorDriver.h"

ClassIdentifier MotorDriver::info ={.name = "None" , .id=0, .unique = '0', .hidden = false};

const ClassIdentifier MotorDriver::getInfo(){
	return info;
}

MotorDriver::MotorDriver() {

}

MotorDriver::~MotorDriver() {

}


void MotorDriver::turn(int16_t val){

}

void MotorDriver::startMotor(){

}
void MotorDriver::stopMotor(){

}

void MotorDriver::emergencyStop(){
	stopMotor();
}

bool MotorDriver::motorReady(){
	return true;
}
