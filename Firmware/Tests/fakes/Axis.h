/*
 * Axis.h (test double)
 *
 * Replaces FFBoard/Inc/Axis.h for sources compiled into the unit test executable.
 * The test include directories are searched before the firmware include directories
 * so `#include "Axis.h"` in EffectsCalculator.cpp resolves to this file.
 *
 * The real Axis pulls in motor drivers, encoders, timers and most of the HAL. For the
 * effect calculation only the interface below is relevant: the calculator reads the axis
 * metrics and writes the resulting effect torque back. This double gives the tests full
 * control over the metrics and records what the calculator writes.
 *
 * If EffectsCalculator starts using more of the Axis interface the build fails here and
 * the missing member has to be added.
 */
#ifndef SRC_AXIS_H_ // Same guard as the real header
#define SRC_AXIS_H_

#include <FFBoardMain.h>
#include "HidFFB.h"
#include "ffb_defs.h"
#include "EffectsCalculator.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

/**
 * Must match metric_t in the real Axis.h
 */
struct metric_t {
	float accel = 0;			//!< Acceleration in deg/s².
	float speed = 0;			//!< Speed in deg/s.
	int32_t pos_scaled_16b = 0; //!< Scaled position as a 16-bit integer (-0x7fff to 0x7fff).
	float pos_f = 0;			//!< Scaled position as a float (-1.0 to 1.0).
	float posDegrees = 0;		//!< Position in degrees, not scaled to the selected range.
	int32_t torque = 0;			//!< Total torque applied to the axis.
};

class Axis {
public:
	// --- Interface used by EffectsCalculator ---
	metric_t *getMetrics() { return &metrics; }

	void calculateMechanicalEffects(bool ffb_on) {
		mechanicalEffectsCalls++;
		lastFfbOn = ffb_on;
	}

	void setFfbEffectTorque(int32_t torque) {
		ffbEffectTorque = torque;
		torqueHistory.push_back(torque);
	}

	// --- Test access ---
	metric_t metrics;					//!< Set by the test before calculating effects
	int32_t ffbEffectTorque = 0;		//!< Last torque written by the effects calculator
	std::vector<int32_t> torqueHistory; //!< All torques written by the effects calculator
	uint32_t mechanicalEffectsCalls = 0;
	bool lastFfbOn = false;
};

#endif /* SRC_AXIS_H_ */
