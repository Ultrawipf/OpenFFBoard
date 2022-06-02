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
#include "PersistentStorage.h"
#include "Encoder.h"
#include "memory"

class Encoder;
class MotorDriver : public ChoosableClass{
public:
	MotorDriver(){};
	virtual ~MotorDriver(){};

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	const ClassType getClassType() override {return ClassType::Motordriver;};
	static const std::vector<class_entry<MotorDriver>> all_drivers;

	virtual void turn(int16_t power);
	virtual void stopMotor();
	virtual void startMotor();
	virtual void emergencyStop(bool reset = false);

	virtual bool motorReady(); // Returns true if the driver is active and ready to receive commands

	virtual Encoder* getEncoder(); // Encoder is managed by the motor driver. Must always return an encoder

	/**
	 * Can pass an external encoder if driver has no integrated encoder
	 * This allows a driver to get an external encoder assigned if it requires one and has the capability of using external encoders
	 */
	virtual void setEncoder(std::shared_ptr<Encoder>& encoder){drvEncoder = encoder;}
	virtual bool hasIntegratedEncoder(); // Returns true if the driver has an integrated encoder. If false the axis will pass one to the driver


protected:
	std::shared_ptr<Encoder> drvEncoder = std::make_shared<Encoder>(); // Dummy encoder
};



#endif /* MOTORDRIVER_H_ */
