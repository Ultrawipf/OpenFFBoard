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

#define MAX_SLEW_RATE 65535

class Encoder;

/**
 * @brief Base class for all motor drivers.
 * This class defines the interface for controlling a motor and optionally its integrated encoder.
 */
class MotorDriver : public ChoosableClass{
public:
	MotorDriver(){};
	virtual ~MotorDriver(){};

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	const ClassType getClassType() override {return ClassType::Motordriver;};
	static const std::vector<class_entry<MotorDriver>> all_drivers;

	/**
	 * @brief Sends a torque command to the motor.
	 * @param power The torque value (signed 16-bit).
	 */
	virtual void turn(int16_t power);
	/**
	 * @brief Stops the motor (torque = 0).
	 */
	virtual void stopMotor();
	/**
	 * @brief Starts the motor driver.
	 */
	virtual void startMotor();
	/**
	 * @brief Triggers an emergency stop.
	 * @param reset If true, resets the driver after stopping.
	 */
	virtual void emergencyStop(bool reset = false);

	/**
	 * @brief Checks if the driver is ready to receive torque commands.
	 * @return true if ready, false otherwise.
	 */
	virtual bool motorReady();

	/**
	 * @brief Gets the encoder managed by this driver.
	 * @return A pointer to the encoder instance.
	 */
	virtual Encoder* getEncoder();

	/**
	 * @brief Sets an external encoder for drivers that don't have an integrated one.
	 * @param encoder A shared pointer to the external encoder.
	 */
	virtual void setEncoder(std::shared_ptr<Encoder>& encoder){drvEncoder = encoder;}

	/**
	 * @brief Checks if the driver has an integrated encoder.
	 * @return true if integrated, false if external.
	 */
	virtual bool hasIntegratedEncoder();

	/**
	 * @brief Gets the hardware measured maximum slew rate of the driver.
	 * @return The maximum slew rate in units/ms.
	 */
	virtual uint32_t getDrvSlewRate() { return MAX_SLEW_RATE; }

	/**
	 * @brief Starts a calibration procedure to measure the driver's max slew rate.
	 * @return true if calibration started successfully.
	 */
	virtual bool startSlewRateCalibration() { return false; }

	/**
	 * @brief Performs initial setup and configuration of the driver.
	 */
	virtual void setupDriver() {}

	/**
	 * @brief Sets the maximum power/current limit for the driver.
	 * @param power The power limit value.
	 */
	virtual void setPowerLimit(uint16_t power) {}


protected:
	std::shared_ptr<Encoder> drvEncoder = std::make_shared<Encoder>(); //!< Pointer to the encoder (integrated or external).
};



#endif /* MOTORDRIVER_H_ */
