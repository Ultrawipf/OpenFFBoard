/*
 * MotorDriver.h
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#ifndef MOTORDRIVER_H_
#define MOTORDRIVER_H_

#include "cppmain.h"
#include "ChoosableClass.h"

class MotorDriver : public ChoosableClass {
public:
	MotorDriver();
	virtual ~MotorDriver();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();


	virtual void turn(int16_t power); // Turn the motor with positive/negative power
	virtual void stop();
	virtual void start();
};

#endif /* MOTORDRIVER_H_ */
