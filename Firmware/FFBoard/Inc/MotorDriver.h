/*
 * MotorDriver.h
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick, Vincent
 */

#ifndef MOTORDRIVER_H_
#define MOTORDRIVER_H_

#define MAX_SLEW_RATE 65535

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

	virtual void setPowerLimit(uint16_t power){}; // specific motor driver manager power, this is used to send powerLimit to the driver, like TMC4671.
	
	// 
	/**
	 * Slew rate calibration interface (no-op by default)
	 * If driver can't calibration the max slew rate will by MAX_SLEW_RATE (65535)
	 * @return false is the driver not implemented this process or true if calibration start
	 */
	virtual bool startSlewRateCalibration() { return false; };
	/**
	 * Check if calibration is in process
	 * @return the state of calibration process
	 */
	virtual bool isSlewRateCalibrationInProgress() { return false; };
	/**
	 * Get the value of the Slew Rate after a calibration
	 * @return 
	 */
	virtual uint16_t getDrvSlewRate() { return MAX_SLEW_RATE; };
	
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
