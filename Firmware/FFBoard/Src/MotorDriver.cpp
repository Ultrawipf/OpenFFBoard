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
#include "VescCAN.h"
#include "MotorSimplemotion.h"
#include "RmdMotorCAN.h"

ClassIdentifier MotorDriver::info ={.name = "None" , .id=CLSID_MOT_NONE, .visibility = ClassVisibility::visible};

/**
 * Add available motor drivers here.
 * ID must be unique to a motor driver. 0-63
 */
const std::vector<class_entry<MotorDriver>> MotorDriver::all_drivers =
{
	add_class<MotorDriver, MotorDriver>(0),

#ifdef TMC4671DRIVER

//		add_class<TMC4671, MotorDriver>(),
	add_class<TMC_1, MotorDriver>(1),
	add_class<TMC_2, MotorDriver>(2),
//		add_class<TMC_3, MotorDriver>(),
#endif
#ifdef PWMDRIVER
	add_class<MotorPWM, MotorDriver>(4),
#endif
#ifdef ODRIVE
	add_class<ODriveCAN1,MotorDriver>(5),
	add_class<ODriveCAN2,MotorDriver>(6),
#endif
#ifdef VESC
	add_class<VESC_1,MotorDriver>(7),
	add_class<VESC_2,MotorDriver>(8),
#endif
#ifdef SIMPLEMOTION
	add_class<MotorSimplemotion1,MotorDriver>(9),
	add_class<MotorSimplemotion2,MotorDriver>(10),
#endif
#ifdef RMDCAN
	add_class<RmdMotorCAN1,MotorDriver>(11),
	add_class<RmdMotorCAN2,MotorDriver>(12),
#endif
};

/**
 * Request an emergency stop if something critical happened or the emergency button is triggered
 * Should stop the motor immediately in a safe way.
 */
void MotorDriver::emergencyStop(bool reset){
	if(reset){
		startMotor();
	}else{
		stopMotor();
	}
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
	turn(0);
}

/**
 * Returns the encoder of this motor driver.
 * Either the integrated encoder or an external encoder assigned to this motor driver passed externally
 */
Encoder* MotorDriver::getEncoder(){
	return this->drvEncoder.get();
}
