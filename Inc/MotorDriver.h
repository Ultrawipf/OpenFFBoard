/*
 * MotorDriver.h
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#ifndef MOTORDRIVER_H_
#define MOTORDRIVER_H_

#include "cppmain.h"
class MotorDriver {
public:
	MotorDriver();
	virtual ~MotorDriver();

	virtual void turn(int16_t power) = 0; // Turn the motor with positive/negative power
	virtual void stop() = 0;
	virtual void start() = 0;
};

#endif /* MOTORDRIVER_H_ */
