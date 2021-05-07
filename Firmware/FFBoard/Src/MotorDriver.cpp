/*
 * MotorDriver.cpp
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#include "MotorDriver.h"
#include "ClassChooser.h"

#include "TMC4671.h"
#include "MotorPWM.h"

ClassIdentifier MotorDriver::info ={.name = "None" , .id=0, .unique = '0', .hidden = false};

/*
 * Add available motor drivers here.
 * ID must be unique to a motor driver. 0-63
 */
const std::vector<class_entry<MotorDriver>> MotorDriver::all_drivers =
{
	add_class<MotorDriver, MotorDriver>(),

#ifdef TMC4671DRIVER

//		add_class<TMC4671, MotorDriver>(),
	add_class<TMC_1, MotorDriver>(),
	add_class<TMC_2, MotorDriver>(),
//		add_class<TMC_3, MotorDriver>(),
#endif
#ifdef PWMDRIVER
	add_class<MotorPWM, MotorDriver>(),
#endif
};


void MotorDriver::emergencyStop(){
	stopMotor();
}

bool MotorDriver::motorReady(){
	return true;
}

bool MotorDriver::hasIntegratedEncoder(){
	return false;
}


const ClassIdentifier MotorDriver::getInfo(){
	return info;
}



void MotorDriver::turn(int16_t val){

}

void MotorDriver::startMotor(){

}
void MotorDriver::stopMotor(){

}

Encoder* MotorDriver::getEncoder(){
	return this->drvEncoder.get();
}
