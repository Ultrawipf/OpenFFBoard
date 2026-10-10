/*
 * MockMotorDriver.h
 *
 * Motor driver implementing the MotorDriver class interface without hardware.
 * Records everything the firmware requests from a driver and lets the test
 * control what the driver reports back.
 */
#ifndef HOSTTEST_MOCKMOTORDRIVER_H_
#define HOSTTEST_MOCKMOTORDRIVER_H_

#include "MotorDriver.h"
#include "ClassChooser.h"
#include "MockEncoder.h"
#include <vector>

class MockMotorDriver : public MotorDriver {
public:
	static constexpr uint16_t SELECTION_ID = 60; //!< Class chooser id. Not used by any firmware driver

	MockMotorDriver() { instances()++; }
	~MockMotorDriver() override { instances()--; }

	static inline ClassIdentifier info = {.name = "Mock driver", .id = CLSID_CUSTOM, .visibility = ClassVisibility::visible};
	const ClassIdentifier getInfo() override { return info; }
	static bool isCreatable() { return creatable(); }

	// --- MotorDriver interface ---
	void setupDriver() override { setupCalls++; }

	void turn(int16_t power) override {
		lastTorque = power;
		torqueHistory.push_back(power);
	}

	void startMotor() override {
		running = true;
		startCalls++;
	}

	void stopMotor() override {
		running = false;
		stopCalls++;
		MotorDriver::stopMotor(); // Base class sets torque 0
	}

	void emergencyStop(bool reset = false) override {
		emergencyStopCalls++;
		emergency = !reset;
		MotorDriver::emergencyStop(reset);
	}

	void setPowerLimit(uint16_t power) override { powerLimit = power; }
	bool motorReady() override { return ready; }
	bool hasIntegratedEncoder() override { return integratedEncoder; }

	bool startSlewRateCalibration() override {
		slewCalibrationRequests++;
		slewCalibrationInProgress = supportsSlewCalibration;
		return supportsSlewCalibration;
	}
	bool isSlewRateCalibrationInProgress() override { return slewCalibrationInProgress; }
	uint16_t getDrvSlewRate() override { return slewRate; }

	// --- Test helpers ---
	/** Makes the driver provide its own encoder like TMC or ODrive do. Returns the encoder for the test to control */
	std::shared_ptr<MockEncoder> useIntegratedEncoder() {
		auto encoder = std::make_shared<MockEncoder>();
		drvEncoder = encoder;
		integratedEncoder = true;
		return encoder;
	}

	/** The encoder currently assigned to the driver (dummy, external or integrated) */
	std::shared_ptr<Encoder> assignedEncoder() { return drvEncoder; }

	/** Class registry entry to make the mock selectable by a ClassChooser<MotorDriver> */
	static class_entry<MotorDriver> classEntry() { return add_class<MockMotorDriver, MotorDriver>(SELECTION_ID); }

	/** Amount of currently existing mock drivers. Detects leaked or double created drivers */
	static int &instances() {
		static int count = 0;
		return count;
	}
	/** Result of isCreatable(). Simulates occupied resources */
	static bool &creatable() {
		static bool value = true;
		return value;
	}

	// --- Recorded calls ---
	int16_t lastTorque = 0;
	std::vector<int16_t> torqueHistory;
	bool running = false;
	bool emergency = false;
	uint16_t powerLimit = 0;
	uint32_t setupCalls = 0;
	uint32_t startCalls = 0;
	uint32_t stopCalls = 0;
	uint32_t emergencyStopCalls = 0;
	uint32_t slewCalibrationRequests = 0;

	// --- Controlled by the test ---
	bool ready = true;
	bool integratedEncoder = false;
	bool supportsSlewCalibration = false;
	bool slewCalibrationInProgress = false;
	uint16_t slewRate = MAX_SLEW_RATE;
};

#endif
