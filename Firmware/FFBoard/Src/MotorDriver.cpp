/*
 * MotorDriver.cpp
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#include <ODriveCAN.h>
#include "MotorDriver.h"
#include "ClassChooser.h"

#include "TMC4671.h"
#include "MotorPWM.h"


ClassIdentifier MotorDriver::info ={.name = "None" , .id=0, .unique = '0', .hidden = false};

/**
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
#ifdef ODRIVE
	add_class<ODriveCAN1,MotorDriver>(),
	add_class<ODriveCAN2,MotorDriver>(),
#endif
};

/**
 * Request an emergency stop if something critical happened or the emergency button is triggered
 * Should stop the motor immediately in a safe way.
 */
void MotorDriver::emergencyStop(){
	stopMotor();
}

bool MotorDriver::motorReady(){
	return true;
}

/**
 * If returned true it signals that this motor driver contains its own encoder and does not require an external encoder
 */
bool MotorDriver::hasIntegratedEncoder(){
	return false;
}


const ClassIdentifier MotorDriver::getInfo(){
	return info;
}


/**
 * Turn the motor with positive/negative power.
 * Range should be full signed 16 bit
 * A value of 0 should have no torque. The sign is the direction.
 */
void MotorDriver::turn(int16_t val){

}

/**
 * Enable the motor driver
 */
void MotorDriver::startMotor(){

}
/**
 * Disable the motor driver
 */
void MotorDriver::stopMotor(){

}

/**
 * Returns the encoder of this motor driver.
 * Either the integrated encoder or an external encoder assigned to this motor driver passed externally
 */
Encoder* MotorDriver::getEncoder(){
	return this->drvEncoder.get();
}
