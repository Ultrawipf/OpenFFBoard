/*
 * Effect behaviour and force calculation (EffectsCalculator.cpp)
 *
 * Effects are created by a simulated PID host (PidHost) through the real HID PID handler.
 * The calculator output is observed at the axis test doubles (fakes/Axis.h):
 * the test sets the axis metrics (position, speed, acceleration) and reads the torque
 * the calculator passes to the axis. No motor driver or encoder is involved.
 *
 * Sign convention of the firmware: a positive force of an effect results in a negative axis torque.
 *
 * Test cases named "characterization" document the current behaviour of the firmware
 * for later verification. They do not claim it is correct.
 */
#include "doctest.h"
#include "TestHelpers.h"
#include "FfbFixture.h"

namespace {

/** Starts a constant force effect with full magnitude on the X axis */
struct ConstantForceFixture : FfbFixture {
	uint8_t cf = 0;
	ConstantForceFixture() {
		cf = startEffect(FFB_EFFECT_CONSTANT);
		host.setConstantForce(cf, 10000);
	}
};

/** Starts a periodic effect with 100ms period and magnitude 10000 */
struct PeriodicFixture : FfbFixture {
	uint8_t startPeriodic(uint8_t type, uint16_t magnitude = 10000, int16_t offset = 0, uint16_t phase = 0, uint32_t period = 100) {
		uint8_t index = startEffect(type);
		host.setPeriodic(index, magnitude, offset, phase, period);
		return index;
	}

	/** Torque on the X axis at a time in ms after the effect start */
	int32_t torqueAt(uint32_t ms) {
		HostPlatform::setTimeMs(ms);
		return torque(0);
	}
};

/** Creates a condition effect acting on the X axis with coefficient 10000 and saturation 10000 */
struct ConditionFixture : FfbFixture {
	uint8_t startCondition(uint8_t type, int16_t coefficient = 10000, uint16_t saturation = 10000, uint16_t deadBand = 0, int16_t cpOffset = 0) {
		uint8_t index = startEffect(type);
		host.setCondition(index, 0, cpOffset, coefficient, coefficient, saturation, saturation, deadBand);
		return index;
	}

	/** Runs the calculation until the effect filters are settled and returns the X axis torque */
	int32_t settledTorque(uint8_t axis = 0) {
		run(1500);
		return axes.at(axis)->ffbEffectTorque;
	}
};

} // namespace

TEST_SUITE("EffectsCalculator") {

// ---------------------------------------------------------------------------
// Activation and axis interface
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(FfbFixture, "inactive calculator outputs zero torque on all axes") {
	axes[0]->ffbEffectTorque = 123;
	axes[1]->ffbEffectTorque = 456;
	calculate();

	CHECK(axes[0]->ffbEffectTorque == 0);
	CHECK(axes[1]->ffbEffectTorque == 0);
	CHECK(axes[0]->mechanicalEffectsCalls == 1);
	CHECK(axes[1]->mechanicalEffectsCalls == 1);
	CHECK_FALSE(axes[0]->lastFfbOn);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "axes are updated every cycle with the ffb state") {
	calculate();
	calculate();
	CHECK(axes[0]->mechanicalEffectsCalls == 2);
	CHECK(axes[0]->torqueHistory.size() == 2);
	CHECK(axes[0]->lastFfbOn);
}

TEST_CASE_FIXTURE(FfbFixture, "active state controls the clip led") {
	ec->setActive(true);
	CHECK(HostPlatform::leds().clipLed);
	ec->setActive(false);
	CHECK_FALSE(HostPlatform::leds().clipLed);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "paused device outputs no force and continues afterwards") {
	CHECK(torque() == -10000);
	host.deviceControl(PidHost::DC_DEVICE_PAUSE);
	CHECK(torque() == 0);
	host.deviceControl(PidHost::DC_DEVICE_CONTINUE);
	CHECK(torque() == -10000);
}

// ---------------------------------------------------------------------------
// Constant force, gains and direction
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ConstantForceFixture, "constant force is passed to the axis with inverted sign") {
	CHECK(torque(0) == -10000);
	CHECK(torque(1) == 0);

	host.setConstantForce(cf, -10000);
	CHECK(torque(0) == 10000);

	host.setConstantForce(cf, 32767);
	CHECK(torque(0) == -32767);

	host.setConstantForce(cf, 0);
	CHECK(torque(0) == 0);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "only started effects create force") {
	host.stopEffect(cf);
	CHECK(torque() == 0);
	host.startEffect(cf);
	CHECK(torque() == -10000);
	host.blockFree(cf);
	CHECK(torque() == 0);
}

TEST_CASE_FIXTURE(FfbFixture, "effect without parameters creates no force") {
	startEffect(FFB_EFFECT_CONSTANT);
	CHECK(torque() == 0);
}

TEST_CASE_FIXTURE(FfbFixture, "effect gain scales the force") {
	PidHost::EffectParams params;
	params.gain = 128;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, 10000);
	CHECK(torque() == -5019); // 10000 * 128 / 255

	params.gain = 0;
	host.setEffect(index, FFB_EFFECT_CONSTANT, params);
	CHECK(torque() == 0);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "device gain scales the force") {
	host.deviceGain(128);
	CHECK(torque() == -5019); // 10000 * 128 / 255
	host.deviceGain(0);
	CHECK(torque() == 0);
	host.deviceGain(255);
	CHECK(torque() == -10000);
}

TEST_CASE_FIXTURE(FfbFixture, "direction distributes the force to the axes") {
	uint8_t index = host.allocateEffect(FFB_EFFECT_CONSTANT);
	host.startEffect(index);
	host.setConstantForce(index, 10000);
	PidHost::EffectParams params;
	params.enableAxis = PidHost::DIRECTION_ENABLE_2AXIS;

	params.directionX = 9000; // Only X
	host.setEffect(index, FFB_EFFECT_CONSTANT, params);
	CHECK_NEAR(torque(0), -10000, 1);
	CHECK_NEAR(torque(1), 0, 1);

	params.directionX = 18000; // Only Y
	host.setEffect(index, FFB_EFFECT_CONSTANT, params);
	CHECK_NEAR(torque(0), 0, 1);
	CHECK_NEAR(torque(1), -10000, 1);

	params.directionX = 4500; // Both
	host.setEffect(index, FFB_EFFECT_CONSTANT, params);
	CHECK_NEAR(torque(0), -7071, 1);
	CHECK_NEAR(torque(1), 7071, 1);

	params.directionX = 27000; // Negative X
	host.setEffect(index, FFB_EFFECT_CONSTANT, params);
	CHECK_NEAR(torque(0), 10000, 1);
}

TEST_CASE_FIXTURE(FfbFixture, "forces of multiple effects are summed without clipping") {
	uint8_t a = startEffect(FFB_EFFECT_CONSTANT);
	uint8_t b = startEffect(FFB_EFFECT_CONSTANT);
	uint8_t c = startEffect(FFB_EFFECT_CONSTANT);
	host.setConstantForce(a, 10000);
	host.setConstantForce(b, 5000);
	CHECK(torque() == -15000);

	host.setConstantForce(c, -15000);
	CHECK(torque() == 0);

	// Clipping is done by the axis
	host.setConstantForce(a, 32767);
	host.setConstantForce(b, 32767);
	host.setConstantForce(c, 32767);
	CHECK(torque() == -3 * 32767);
}

TEST_CASE_FIXTURE(FfbFixture1Axis, "single axis device") {
	PidHost::EffectParams params;
	params.enableAxis = PidHost::AXIS_X | PidHost::DIRECTION_ENABLE_1AXIS;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, 10000);
	CHECK(torque(0) == -10000);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "constant force filter is bypassed at the default frequency") {
	// Default 500Hz is half the update rate which disables the filter. Output follows immediately
	host.setConstantForce(cf, 20000);
	CHECK(torque() == -20000);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "constant force lowpass filter smooths steps and settles at the target") {
	fxCommand(EffectsCalculator_commands::ffbfiltercf, CMDtype::set, 50);
	calculate();
	int32_t first = axes[0]->ffbEffectTorque;
	CHECK(first < 0);
	CHECK(first > -5000); // Step is not passed directly

	run(500);
	CHECK_NEAR(axes[0]->ffbEffectTorque, -10000, 20);
}

// ---------------------------------------------------------------------------
// Duration and start delay
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(FfbFixture, "effect ends after its duration") {
	PidHost::EffectParams params;
	params.duration = 100;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, 10000);

	HostPlatform::setTimeMs(50);
	CHECK(torque() == -10000);
	HostPlatform::setTimeMs(100);
	CHECK(torque() == -10000);
	CHECK(effect(index).state == 1);

	HostPlatform::setTimeMs(101);
	CHECK(torque() == 0);
	CHECK(effect(index).state == 0);
	CHECK(effect(index).type == FFB_EFFECT_CONSTANT); // Still allocated and can be restarted

	host.startEffect(index);
	CHECK(torque() == -10000);
}

TEST_CASE_FIXTURE(FfbFixture, "infinite effects never end") {
	PidHost::EffectParams params;
	params.duration = 0xFFFF;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, 10000);
	HostPlatform::setTimeMs(10 * 60 * 1000);
	CHECK(torque() == -10000);
}

TEST_CASE_FIXTURE(FfbFixture, "start delay postpones an effect with duration") {
	PidHost::EffectParams params;
	params.duration = 100;
	params.startDelay = 50;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, 10000);

	HostPlatform::setTimeMs(49);
	CHECK(torque() == 0);
	CHECK(effect(index).state == 1);
	HostPlatform::setTimeMs(50);
	CHECK(torque() == -10000);
	HostPlatform::setTimeMs(150);
	CHECK(torque() == -10000);
	HostPlatform::setTimeMs(151);
	CHECK(torque() == 0);
}

TEST_CASE_FIXTURE(FfbFixture, "start delay postpones an infinite effect" *
		KNOWN_ISSUE("EffectsCalculator::calculateEffects only checks the start time of effects with a finite duration")) {
	PidHost::EffectParams params;
	params.startDelay = 50;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, 10000);

	HostPlatform::setTimeMs(10);
	CHECK(torque() == 0);
	HostPlatform::setTimeMs(50);
	CHECK(torque() == -10000);
}

// ---------------------------------------------------------------------------
// Ramp
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(PeriodicFixture, "ramp changes linear from start to end level over the duration") {
	PidHost::EffectParams params;
	params.duration = 1000;
	uint8_t index = startEffect(FFB_EFFECT_RAMP, params);
	host.setRamp(index, -10000, 10000);

	CHECK_NEAR(torqueAt(0), 10000, 2);
	CHECK_NEAR(torqueAt(250), 5000, 2);
	CHECK_NEAR(torqueAt(500), 0, 2);
	CHECK_NEAR(torqueAt(750), -5000, 2);
	CHECK_NEAR(torqueAt(1000), -10000, 2);
	CHECK(torqueAt(1001) == 0); // Ended
}

TEST_CASE_FIXTURE(PeriodicFixture, "ramp is scaled by the effect gain") {
	PidHost::EffectParams params;
	params.duration = 1000;
	params.gain = 128;
	uint8_t index = startEffect(FFB_EFFECT_RAMP, params);
	host.setRamp(index, 10000, 10000);
	CHECK_NEAR(torqueAt(500), -5019, 2);
}

// ---------------------------------------------------------------------------
// Periodic effects. Period 100ms, magnitude 10000
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(PeriodicFixture, "sine") {
	startPeriodic(FFB_EFFECT_SINE);
	CHECK_NEAR(torqueAt(0), 0, 5);
	CHECK_NEAR(torqueAt(25), -10000, 5);
	CHECK_NEAR(torqueAt(50), 0, 5);
	CHECK_NEAR(torqueAt(75), 10000, 5);
	CHECK_NEAR(torqueAt(100), 0, 5);
	CHECK_NEAR(torqueAt(1025), -10000, 5); // Periodic
}

TEST_CASE_FIXTURE(PeriodicFixture, "sine offset shifts the center") {
	startPeriodic(FFB_EFFECT_SINE, 10000, 2000);
	CHECK_NEAR(torqueAt(0), -2000, 5);
	CHECK_NEAR(torqueAt(25), -12000, 5);
	CHECK_NEAR(torqueAt(75), 8000, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "sine phase shifts the start") {
	startPeriodic(FFB_EFFECT_SINE, 10000, 0, 9000); // 90 degrees
	CHECK_NEAR(torqueAt(0), -10000, 5);
	CHECK_NEAR(torqueAt(25), 0, 5);
	CHECK_NEAR(torqueAt(50), 10000, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "sine period") {
	startPeriodic(FFB_EFFECT_SINE, 10000, 0, 0, 1000);
	CHECK_NEAR(torqueAt(250), -10000, 5);
	CHECK_NEAR(torqueAt(750), 10000, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "sine time is relative to the effect start") {
	HostPlatform::setTimeMs(12345);
	startPeriodic(FFB_EFFECT_SINE);
	CHECK_NEAR(torqueAt(12345), 0, 5);
	CHECK_NEAR(torqueAt(12345 + 25), -10000, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "periodic effects are scaled by effect and device gain") {
	PidHost::EffectParams params;
	params.gain = 128;
	uint8_t index = startEffect(FFB_EFFECT_SINE, params);
	host.setPeriodic(index, 10000, 0, 0, 100);
	CHECK_NEAR(torqueAt(25), -5019, 5);
	host.deviceGain(128);
	CHECK_NEAR(torqueAt(25), -2519, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "characterization: square starts with the negative half") {
	startPeriodic(FFB_EFFECT_SQUARE, 5000);
	CHECK(torqueAt(0) == 5000);
	CHECK(torqueAt(25) == 5000);
	CHECK(torqueAt(75) == -5000);
}

TEST_CASE_FIXTURE(PeriodicFixture, "square offset shifts the center") {
	startPeriodic(FFB_EFFECT_SQUARE, 5000, 1000);
	CHECK(torqueAt(25) == 4000);
	CHECK(torqueAt(75) == -6000);
}

TEST_CASE_FIXTURE(PeriodicFixture, "square repeats with the effect period" *
		KNOWN_ISSUE("The square wave is calculated with period+2 ms. A 100ms effect repeats every 102ms and drifts against the other periodic effects")) {
	startPeriodic(FFB_EFFECT_SQUARE, 5000);
	for (uint32_t cycle = 0; cycle < 20; cycle++) {
		CAPTURE(cycle);
		CHECK(torqueAt(cycle * 100 + 25) == 5000);
		CHECK(torqueAt(cycle * 100 + 75) == -5000);
	}
}

TEST_CASE_FIXTURE(PeriodicFixture, "triangle") {
	startPeriodic(FFB_EFFECT_TRIANGLE);
	CHECK_NEAR(torqueAt(0), 10000, 5);
	CHECK_NEAR(torqueAt(25), 0, 5);
	CHECK_NEAR(torqueAt(50), -10000, 5);
	CHECK_NEAR(torqueAt(75), 0, 5);
	CHECK_NEAR(torqueAt(100), 10000, 5);
	CHECK_NEAR(torqueAt(1050), -10000, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "triangle offset shifts the center") {
	startPeriodic(FFB_EFFECT_TRIANGLE, 10000, 2000);
	CHECK_NEAR(torqueAt(0), 8000, 5);
	CHECK_NEAR(torqueAt(50), -12000, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "characterization: sawtooth up") {
	// Current implementation: force starts at the maximum and falls to the minimum
	startPeriodic(FFB_EFFECT_SAWTOOTHUP);
	CHECK_NEAR(torqueAt(0), -10000, 5);
	CHECK_NEAR(torqueAt(25), -5000, 5);
	CHECK_NEAR(torqueAt(50), 0, 5);
	CHECK_NEAR(torqueAt(99), 9800, 5);
	CHECK_NEAR(torqueAt(100), -10000, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "characterization: sawtooth down") {
	// Current implementation: force starts at the minimum and rises to the maximum
	startPeriodic(FFB_EFFECT_SAWTOOTHDOWN);
	CHECK_NEAR(torqueAt(0), 10000, 5);
	CHECK_NEAR(torqueAt(25), 5000, 5);
	CHECK_NEAR(torqueAt(50), 0, 5);
	CHECK_NEAR(torqueAt(99), -9800, 5);
	CHECK_NEAR(torqueAt(100), 10000, 5);
}

TEST_CASE_FIXTURE(PeriodicFixture, "sawtooth up rises during the period in the sign convention of the other effects" *
		KNOWN_ISSUE("To be verified: sawtooth up and down appear swapped. A constant force or the first half of the sine with positive magnitude create negative torque, a rising sawtooth should therefore go from positive to negative torque")) {
	startPeriodic(FFB_EFFECT_SAWTOOTHUP);
	CHECK(torqueAt(10) > torqueAt(90));
}

// ---------------------------------------------------------------------------
// Envelope
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(PeriodicFixture, "envelope attack and fade on a constant force") {
	PidHost::EffectParams params;
	params.duration = 1000;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, 10000);
	host.setEnvelope(index, 0, 0, 100, 200); // Attack 100ms from 0, fade 200ms to 0

	CHECK_NEAR(torqueAt(0), 0, 2);
	CHECK_NEAR(torqueAt(50), -5000, 2);
	CHECK_NEAR(torqueAt(100), -10000, 2);
	CHECK_NEAR(torqueAt(500), -10000, 2); // Sustain
	CHECK_NEAR(torqueAt(800), -10000, 2);
	CHECK_NEAR(torqueAt(900), -5000, 2);
	CHECK_NEAR(torqueAt(1000), 0, 2);
}

TEST_CASE_FIXTURE(PeriodicFixture, "envelope levels above and below the magnitude") {
	PidHost::EffectParams params;
	params.duration = 1000;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, 10000);
	host.setEnvelope(index, 20000, 5000, 100, 100);

	CHECK_NEAR(torqueAt(0), -20000, 2);
	CHECK_NEAR(torqueAt(50), -15000, 2);
	CHECK_NEAR(torqueAt(500), -10000, 2);
	CHECK_NEAR(torqueAt(950), -7500, 2);
	CHECK_NEAR(torqueAt(1000), -5000, 2);
}

TEST_CASE_FIXTURE(PeriodicFixture, "envelope keeps the sign of a negative constant force") {
	PidHost::EffectParams params;
	params.duration = 1000;
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT, params);
	host.setConstantForce(index, -10000);
	host.setEnvelope(index, 0, 0, 100, 100);

	CHECK_NEAR(torqueAt(50), 5000, 2);
	CHECK_NEAR(torqueAt(500), 10000, 2);
}

TEST_CASE_FIXTURE(PeriodicFixture, "envelope modulates the amplitude of periodic effects") {
	PidHost::EffectParams params;
	params.duration = 1000;
	uint8_t index = startEffect(FFB_EFFECT_SINE, params);
	host.setPeriodic(index, 10000, 0, 0, 100);
	host.setEnvelope(index, 0, 0, 500, 0);

	CHECK_NEAR(torqueAt(25), -500, 10);	  // 5% of the attack
	CHECK_NEAR(torqueAt(225), -4500, 10); // 45%
	CHECK_NEAR(torqueAt(525), -10000, 10);
}

TEST_CASE_FIXTURE(PeriodicFixture, "envelope is ignored for infinite effects") {
	uint8_t index = startEffect(FFB_EFFECT_CONSTANT);
	host.setConstantForce(index, 10000);
	host.setEnvelope(index, 0, 0, 100, 100);
	CHECK(torqueAt(0) == -10000);
	CHECK(torqueAt(50) == -10000);
}

// ---------------------------------------------------------------------------
// Spring (metric: axis position -32767..32767)
// force = coefficient/32767 * (gain+1)/256 * scaler * (position - offset -+ deadband)
// Defaults: spring gain 64, scaler 16  ->  coefficient 10000: 1.2398 per position count
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ConditionFixture, "spring pushes back to the center proportional to the position") {
	startCondition(FFB_EFFECT_SPRING);

	metrics().pos_scaled_16b = 0;
	CHECK(torque() == 0);
	metrics().pos_scaled_16b = 1000;
	CHECK_NEAR(torque(), -1239, 1);
	metrics().pos_scaled_16b = 2000;
	CHECK_NEAR(torque(), -2479, 1);
	metrics().pos_scaled_16b = -1000;
	CHECK_NEAR(torque(), 1239, 1);
	CHECK(torque(1) == 0); // Other axis not affected
}

TEST_CASE_FIXTURE(ConditionFixture, "spring is independent of speed and acceleration") {
	startCondition(FFB_EFFECT_SPRING);
	metrics().pos_scaled_16b = 1000;
	metrics().speed = 500;
	metrics().accel = 5000;
	CHECK_NEAR(torque(), -1239, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "spring force is limited by the saturation") {
	uint8_t index = startEffect(FFB_EFFECT_SPRING);
	host.setCondition(index, 0, 0, 10000, 10000, 500, 300, 0);

	metrics().pos_scaled_16b = 1000;
	CHECK(torque() == -500); // Positive saturation
	metrics().pos_scaled_16b = 30000;
	CHECK(torque() == -500);
	metrics().pos_scaled_16b = -1000;
	CHECK(torque() == 300); // Negative saturation
	metrics().pos_scaled_16b = 100;
	CHECK_NEAR(torque(), -123, 1); // Below saturation
}

TEST_CASE_FIXTURE(ConditionFixture, "spring uses separate coefficients for both sides") {
	uint8_t index = startEffect(FFB_EFFECT_SPRING);
	host.setCondition(index, 0, 0, 10000, 5000, 10000, 10000, 0);

	metrics().pos_scaled_16b = 1000;
	CHECK_NEAR(torque(), -1239, 1);
	metrics().pos_scaled_16b = -1000;
	CHECK_NEAR(torque(), 619, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "spring dead band") {
	startCondition(FFB_EFFECT_SPRING, 10000, 10000, 200);

	for (int32_t pos : {0, 100, 200, -100, -200}) {
		CAPTURE(pos);
		metrics().pos_scaled_16b = pos;
		CHECK(torque() == 0);
	}
	// Force starts at the edge of the dead band
	metrics().pos_scaled_16b = 1200;
	CHECK_NEAR(torque(), -1239, 1);
	metrics().pos_scaled_16b = -1200;
	CHECK_NEAR(torque(), 1239, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "spring center point offset") {
	startCondition(FFB_EFFECT_SPRING, 10000, 10000, 0, 500);

	metrics().pos_scaled_16b = 500;
	CHECK(torque() == 0);
	metrics().pos_scaled_16b = 1500;
	CHECK_NEAR(torque(), -1239, 1);
	metrics().pos_scaled_16b = -500;
	CHECK_NEAR(torque(), 1239, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "spring with negative coefficient pushes away from the center") {
	startCondition(FFB_EFFECT_SPRING, -10000);
	metrics().pos_scaled_16b = 1000;
	CHECK_NEAR(torque(), 1239, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "characterization: condition without saturation creates no force") {
	// PID hosts may send saturation 0. Currently the force is clipped to 0 in that case
	startCondition(FFB_EFFECT_SPRING, 10000, 0);
	metrics().pos_scaled_16b = 1000;
	CHECK(torque() == 0);
}

TEST_CASE_FIXTURE(ConditionFixture, "spring is scaled by the spring gain setting and the device gain") {
	startCondition(FFB_EFFECT_SPRING);
	metrics().pos_scaled_16b = 1000;

	fxCommand(EffectsCalculator_commands::spring, CMDtype::set, 127);
	CHECK_NEAR(torque(), -2441, 1); // (127+1)/256 * 16 * 10000/32767 * 1000
	fxCommand(EffectsCalculator_commands::spring, CMDtype::set, 0);
	CHECK_NEAR(torque(), -19, 1);

	fxCommand(EffectsCalculator_commands::spring, CMDtype::set, 127);
	host.deviceGain(128);
	CHECK_NEAR(torque(), -1225, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "characterization: condition effects are not scaled by the effect gain") {
	PidHost::EffectParams params;
	params.gain = 0;
	uint8_t index = startEffect(FFB_EFFECT_SPRING, params);
	host.setCondition(index, 0, 0, 10000, 10000, 10000, 10000, 0);
	metrics().pos_scaled_16b = 1000;
	CHECK_NEAR(torque(), -1239, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "two condition blocks act on each axis with their own parameters") {
	uint8_t index = host.allocateEffect(FFB_EFFECT_SPRING);
	host.setCondition(index, 0, 0, 10000, 10000, 10000, 10000, 0);
	host.setCondition(index, 1, 0, 5000, 5000, 10000, 10000, 0);
	host.setEffect(index, FFB_EFFECT_SPRING);
	host.startEffect(index);

	metrics(0).pos_scaled_16b = 1000;
	metrics(1).pos_scaled_16b = 1000;
	calculate();
	CHECK_NEAR(axes[0]->ffbEffectTorque, -1239, 1);
	CHECK_NEAR(axes[1]->ffbEffectTorque, -619, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "two condition blocks act on each axis independent of the report order" *
		KNOWN_ISSUE("The direction is only overridden if the condition blocks are received before the set effect report. In the other order "
					"HidFFB::set_condition keeps the rotated magnitudes unless they are exactly 0")) {
	PidHost::EffectParams params;
	params.enableAxis = PidHost::AXIS_X | PidHost::AXIS_Y | PidHost::DIRECTION_ENABLE_2AXIS;
	params.directionX = 4500;
	uint8_t index = startEffect(FFB_EFFECT_SPRING, params);
	host.setCondition(index, 0, 0, 10000, 10000, 10000, 10000, 0);
	host.setCondition(index, 1, 0, 5000, 5000, 10000, 10000, 0);

	metrics(0).pos_scaled_16b = 1000;
	metrics(1).pos_scaled_16b = 1000;
	calculate();
	CHECK_NEAR(axes[0]->ffbEffectTorque, -1239, 1);
	CHECK_NEAR(axes[1]->ffbEffectTorque, -619, 1);
}

TEST_CASE_FIXTURE(ConditionFixture, "single condition block rotated by 45 degrees acts on both axes") {
	PidHost::EffectParams params;
	params.enableAxis = PidHost::DIRECTION_ENABLE_2AXIS;
	params.directionX = 4500;
	uint8_t index = host.allocateEffect(FFB_EFFECT_SPRING);
	host.setCondition(index, 0, 0, 10000, 10000, 10000, 10000, 0);
	host.setEffect(index, FFB_EFFECT_SPRING, params);
	host.startEffect(index);

	metrics(0).pos_scaled_16b = 1000;
	metrics(1).pos_scaled_16b = 1000;
	calculate();
	CHECK_NEAR(axes[0]->ffbEffectTorque, -876, 2); // 1239 * 0.7071
	CHECK_NEAR(axes[1]->ffbEffectTorque, 876, 2);  // Characterization: sign from the Y direction component
}

// ---------------------------------------------------------------------------
// Damper (metric: axis speed in deg/s * 40)
// Defaults: damper gain 64, scaler 4  ->  coefficient 10000: 12.398 per deg/s
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ConditionFixture, "damper opposes the movement proportional to the speed") {
	startCondition(FFB_EFFECT_DAMPER);

	metrics().speed = 0;
	CHECK(settledTorque() == 0);
	metrics().speed = 10;
	CHECK_NEAR(settledTorque(), -123, 2);
	metrics().speed = 20;
	CHECK_NEAR(settledTorque(), -247, 2);
	metrics().speed = -10;
	CHECK_NEAR(settledTorque(), 123, 2);
}

TEST_CASE_FIXTURE(ConditionFixture, "damper is independent of position and acceleration") {
	startCondition(FFB_EFFECT_DAMPER);
	metrics().pos_scaled_16b = 20000;
	metrics().accel = 5000;
	CHECK(settledTorque() == 0);
}

TEST_CASE_FIXTURE(ConditionFixture, "damper force is limited by the saturation") {
	startCondition(FFB_EFFECT_DAMPER, 10000, 2000);
	metrics().speed = 1000;
	CHECK_NEAR(settledTorque(), -2000, 2);
	metrics().speed = -1000;
	CHECK_NEAR(settledTorque(), 2000, 2);
}

TEST_CASE_FIXTURE(ConditionFixture, "damper output is lowpass filtered") {
	startCondition(FFB_EFFECT_DAMPER);
	metrics().speed = 100;
	calculate();
	int32_t first = axes[0]->ffbEffectTorque;
	CHECK(first <= 0);
	CHECK(first > -600); // Less than half of the final value in the first cycle
	CHECK_NEAR(settledTorque(), -1239, 3);
}

TEST_CASE_FIXTURE(ConditionFixture, "damper is scaled by the damper gain setting") {
	startCondition(FFB_EFFECT_DAMPER);
	metrics().speed = 10;
	fxCommand(EffectsCalculator_commands::damper, CMDtype::set, 127);
	CHECK_NEAR(settledTorque(), -244, 2);
}

// ---------------------------------------------------------------------------
// Inertia (metric: axis acceleration in deg/s² * 4)
// Defaults: inertia gain 127, scaler 2  ->  coefficient 10000: 1.2207 per deg/s²
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ConditionFixture, "inertia opposes the acceleration") {
	startCondition(FFB_EFFECT_INERTIA);

	metrics().accel = 0;
	CHECK(settledTorque() == 0);
	metrics().accel = 1000;
	CHECK_NEAR(settledTorque(), -1220, 2);
	metrics().accel = -1000;
	CHECK_NEAR(settledTorque(), 1220, 2);
}

TEST_CASE_FIXTURE(ConditionFixture, "inertia is independent of position and speed") {
	startCondition(FFB_EFFECT_INERTIA);
	metrics().pos_scaled_16b = 20000;
	metrics().speed = 500;
	CHECK(settledTorque() == 0);
}

// ---------------------------------------------------------------------------
// Friction (metric: axis speed in deg/s * 45)
// Constant force against the direction of movement with a smooth ramp up near zero speed.
// Defaults: friction gain 254, scaler 1, ramp up below 25% of the speed range (182 deg/s)
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ConditionFixture, "friction creates a constant force against the movement above the ramp up speed") {
	startCondition(FFB_EFFECT_FRICTION);

	metrics().speed = 500;
	CHECK_NEAR(settledTorque(), -9960, 3); // coefficient * 255/256
	metrics().speed = 1000;
	CHECK_NEAR(settledTorque(), -9960, 3); // Independent of the speed
	metrics().speed = -500;
	CHECK_NEAR(settledTorque(), 9960, 3);
}

TEST_CASE_FIXTURE(ConditionFixture, "friction creates no force at standstill") {
	startCondition(FFB_EFFECT_FRICTION);
	metrics().speed = 0;
	CHECK(settledTorque() == 0);
}

TEST_CASE_FIXTURE(ConditionFixture, "friction ramps up smoothly at low speed") {
	startCondition(FFB_EFFECT_FRICTION);

	metrics().speed = 91; // Half of the ramp up speed
	CHECK_NEAR(settledTorque(), -4980, 30);

	metrics().speed = 20;
	int32_t slow = settledTorque();
	metrics().speed = 150;
	int32_t fast = settledTorque();
	CHECK(slow < 0);
	CHECK(slow > -1000);
	CHECK(fast < -8000);
	CHECK(fast > -9960);
}

TEST_CASE_FIXTURE(ConditionFixture, "friction ramp up range is configurable") {
	startCondition(FFB_EFFECT_FRICTION);
	fxCommand(EffectsCalculator_commands::frictionPctSpeedToRampup, CMDtype::set, 0);
	metrics().speed = 1;
	CHECK_NEAR(settledTorque(), -9960, 3); // No ramp up
}

TEST_CASE_FIXTURE(ConditionFixture, "friction force is limited by the saturation") {
	startCondition(FFB_EFFECT_FRICTION, 10000, 2000);
	metrics().speed = 500;
	CHECK_NEAR(settledTorque(), -1992, 3); // 2000 * 255/256
}

TEST_CASE_FIXTURE(ConditionFixture, "friction is scaled by the friction gain setting") {
	startCondition(FFB_EFFECT_FRICTION);
	fxCommand(EffectsCalculator_commands::friction, CMDtype::set, 127);
	metrics().speed = 500;
	CHECK_NEAR(settledTorque(), -5000, 3);
}

// ---------------------------------------------------------------------------
// Combination of effects like in a game
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ConditionFixture, "constant force, spring and damper are summed") {
	uint8_t cf = startEffect(FFB_EFFECT_CONSTANT);
	host.setConstantForce(cf, 5000);
	startCondition(FFB_EFFECT_SPRING);
	startCondition(FFB_EFFECT_DAMPER);

	metrics().pos_scaled_16b = 1000;
	metrics().speed = 10;
	CHECK_NEAR(settledTorque(), -5000 - 1239 - 123, 4);
}

// ---------------------------------------------------------------------------
// Reconstruction filter for effect updates
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ConstantForceFixture, "without reconstruction updates are applied immediately") {
	host.setConstantForce(cf, 2000);
	CHECK(torque() == -2000);
	HostPlatform::advanceMs(5);
	host.setConstantForce(cf, 4000);
	CHECK(torque() == -4000);
}

TEST_CASE_FIXTURE(FfbFixture, "linear reconstruction ramps from the previous to the latest value within one update interval") {
	fxCommand(EffectsCalculator_commands::reconFilterMode, CMDtype::set, (int64_t)ReconFilterMode::LINEAR_INTERPOLATION);
	uint8_t cf = startEffect(FFB_EFFECT_CONSTANT);

	HostPlatform::setTimeMs(1000);
	host.setConstantForce(cf, 0);
	HostPlatform::setTimeMs(1010);
	host.setConstantForce(cf, 10000); // 10ms update interval

	CHECK_NEAR(torque(), 0, 2); // Starts at the previous value
	HostPlatform::setTimeMs(1015);
	CHECK_NEAR(torque(), -5000, 20);
	HostPlatform::setTimeMs(1020);
	CHECK_NEAR(torque(), -10000, 20);
	HostPlatform::setTimeMs(1030);
	CHECK_NEAR(torque(), -10000, 2); // No extrapolation
}

TEST_CASE_FIXTURE(FfbFixture, "reconstruction holds the latest value if updates stop") {
	for (ReconFilterMode mode : {ReconFilterMode::LINEAR_INTERPOLATION, ReconFilterMode::SPLINE_CUBIC_NATURAL, ReconFilterMode::SPLINE_CUBIC_HERMITE}) {
		CAPTURE((int)mode);
		fxCommand(EffectsCalculator_commands::reconFilterMode, CMDtype::set, (int64_t)mode);
		uint8_t cf = startEffect(FFB_EFFECT_CONSTANT);

		HostPlatform::advanceMs(1000);
		host.setConstantForce(cf, 0);
		HostPlatform::advanceMs(10);
		host.setConstantForce(cf, 10000);
		HostPlatform::advanceMs(51); // Timeout 50ms
		CHECK(torque() == -10000);
		host.blockFree(cf);
	}
}

TEST_CASE_FIXTURE(FfbFixture, "reconstruction of a constant signal is constant") {
	for (ReconFilterMode mode : {ReconFilterMode::LINEAR_INTERPOLATION, ReconFilterMode::SPLINE_CUBIC_NATURAL, ReconFilterMode::SPLINE_CUBIC_HERMITE}) {
		CAPTURE((int)mode);
		fxCommand(EffectsCalculator_commands::reconFilterMode, CMDtype::set, (int64_t)mode);
		uint8_t cf = startEffect(FFB_EFFECT_CONSTANT);
		HostPlatform::advanceMs(1000);

		for (int update = 0; update < 10; update++) {
			host.setConstantForce(cf, 8000);
			for (int ms = 0; ms < 16; ms++) {
				CHECK_NEAR(torque(), -8000, 1);
				HostPlatform::advanceMs(1);
			}
		}
		host.blockFree(cf);
	}
}

TEST_CASE_FIXTURE(FfbFixture, "reconstruction of a step never leaves the range of the input values and reaches the target") {
	for (ReconFilterMode mode : {ReconFilterMode::LINEAR_INTERPOLATION, ReconFilterMode::SPLINE_CUBIC_HERMITE}) {
		CAPTURE((int)mode);
		fxCommand(EffectsCalculator_commands::reconFilterMode, CMDtype::set, (int64_t)mode);
		uint8_t cf = startEffect(FFB_EFFECT_CONSTANT);
		HostPlatform::advanceMs(1000);

		int32_t last = 0;
		for (int update = 0; update < 12; update++) {
			host.setConstantForce(cf, update < 4 ? 0 : 10000);
			for (int ms = 0; ms < 10; ms++) {
				last = torque();
				CHECK(last <= 600);		// Characterization: The hermite spline overshoots by up to ~6%
				CHECK(last >= -10600);
				HostPlatform::advanceMs(1);
			}
		}
		CHECK_NEAR(last, -10000, 10);
		host.blockFree(cf);
	}
}

// ---------------------------------------------------------------------------
// Settings via commands (fx class)
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(FfbFixture, "effects calculator is addressable as fx class") {
	CHECK(CommandHandler::getHandlerFromClassName("fx") == ec.get());
	CHECK(CommandHandler::getHandlerFromId(CLSID_EFFECTSCALC) == ec.get());
	for (const char *cmd : {"filterCfFreq", "filterCfQ", "spring", "friction", "damper", "inertia", "effects", "effectsDetails", "effectsForces",
							"damper_f", "damper_q", "friction_f", "friction_q", "inertia_f", "inertia_q", "filterProfile_id", "frictionPctSpeedToRampup", "reconFilterMode"}) {
		CAPTURE(std::string(cmd));
		CHECK(ec->getCommandFromName(cmd) != nullptr);
	}
}

TEST_CASE_FIXTURE(FfbFixture, "default gain settings") {
	std::vector<CommandReply> replies;
	fxCommand(EffectsCalculator_commands::spring, CMDtype::get, 0, &replies);
	fxCommand(EffectsCalculator_commands::damper, CMDtype::get, 0, &replies);
	fxCommand(EffectsCalculator_commands::friction, CMDtype::get, 0, &replies);
	fxCommand(EffectsCalculator_commands::inertia, CMDtype::get, 0, &replies);
	REQUIRE(replies.size() == 4);
	CHECK(replies[0].val == 64);
	CHECK(replies[1].val == 64);
	CHECK(replies[2].val == 254);
	CHECK(replies[3].val == 127);
}

TEST_CASE_FIXTURE(FfbFixture, "gain settings can be written and read") {
	for (EffectsCalculator_commands cmd : {EffectsCalculator_commands::spring, EffectsCalculator_commands::damper, EffectsCalculator_commands::friction, EffectsCalculator_commands::inertia}) {
		std::vector<CommandReply> replies;
		CHECK(fxCommand(cmd, CMDtype::set, 99) == CommandStatus::OK);
		CHECK(fxCommand(cmd, CMDtype::get, 0, &replies) == CommandStatus::OK);
		REQUIRE(replies.size() == 1);
		CHECK(replies[0].val == 99);

		replies.clear();
		CHECK(fxCommand(cmd, CMDtype::info, 0, &replies) == CommandStatus::OK);
		REQUIRE(replies.size() == 1);
		CHECK(replies[0].reply.rfind("scale:", 0) == 0);
	}
}

TEST_CASE_FIXTURE(FfbFixture, "settings are limited to their valid range") {
	auto setAndGet = [&](EffectsCalculator_commands cmd, int64_t value) {
		std::vector<CommandReply> replies;
		fxCommand(cmd, CMDtype::set, value);
		fxCommand(cmd, CMDtype::get, 0, &replies);
		REQUIRE(replies.size() == 1);
		return replies[0].val;
	};
	CHECK(setAndGet(EffectsCalculator_commands::ffbfiltercf, 100) == 100);
	CHECK(setAndGet(EffectsCalculator_commands::ffbfiltercf, 9999) == 500); // Half of the update rate
	CHECK(setAndGet(EffectsCalculator_commands::ffbfiltercf, 0) == 500);
	CHECK(setAndGet(EffectsCalculator_commands::ffbfiltercf_q, 50) == 50);
	CHECK(setAndGet(EffectsCalculator_commands::ffbfiltercf_q, 0) == 1);
	CHECK(setAndGet(EffectsCalculator_commands::ffbfiltercf_q, 200) == 127);
	CHECK(setAndGet(EffectsCalculator_commands::frictionPctSpeedToRampup, 50) == 50);
	CHECK(setAndGet(EffectsCalculator_commands::frictionPctSpeedToRampup, 101) == 100);
	CHECK(setAndGet(EffectsCalculator_commands::reconFilterMode, 2) == 2);
	CHECK(setAndGet(EffectsCalculator_commands::reconFilterMode, 9) == 3);
	CHECK(setAndGet(EffectsCalculator_commands::filterProfileId, 1) == 1);
	CHECK(setAndGet(EffectsCalculator_commands::filterProfileId, 5) == 1);
}

TEST_CASE_FIXTURE(FfbFixture, "condition filter settings belong to the custom profile") {
	auto get = [&](EffectsCalculator_commands cmd) {
		std::vector<CommandReply> replies;
		fxCommand(cmd, CMDtype::get, 0, &replies);
		REQUIRE(replies.size() == 1);
		return replies[0].val;
	};
	// Default profile values
	CHECK(get(EffectsCalculator_commands::damper_f) == 30);
	CHECK(get(EffectsCalculator_commands::damper_q) == 40);
	CHECK(get(EffectsCalculator_commands::friction_f) == 50);
	CHECK(get(EffectsCalculator_commands::friction_q) == 20);
	CHECK(get(EffectsCalculator_commands::inertia_f) == 15);
	CHECK(get(EffectsCalculator_commands::inertia_q) == 20);

	// Writes go to the custom profile and are not visible while the default profile is active
	fxCommand(EffectsCalculator_commands::damper_f, CMDtype::set, 80);
	CHECK(get(EffectsCalculator_commands::damper_f) == 30);
	fxCommand(EffectsCalculator_commands::filterProfileId, CMDtype::set, CUSTOM_PROFILE_ID);
	CHECK(get(EffectsCalculator_commands::damper_f) == 80);
}

TEST_CASE_FIXTURE(FfbFixture, "filter settings are applied to existing effects") {
	uint8_t cf = host.allocateEffect(FFB_EFFECT_CONSTANT);
	uint8_t damper = host.allocateEffect(FFB_EFFECT_DAMPER);
	CHECK(effect(cf).filter[0]->getFc() == doctest::Approx(0.5));
	CHECK(effect(damper).filter[0]->getFc() == doctest::Approx(0.03));

	fxCommand(EffectsCalculator_commands::ffbfiltercf, CMDtype::set, 100);
	CHECK(effect(cf).filter[0]->getFc() == doctest::Approx(0.1));
	CHECK(effect(cf).filter[1]->getFc() == doctest::Approx(0.1));

	fxCommand(EffectsCalculator_commands::filterProfileId, CMDtype::set, CUSTOM_PROFILE_ID);
	fxCommand(EffectsCalculator_commands::damper_f, CMDtype::set, 60);
	CHECK(effect(damper).filter[0]->getFc() == doctest::Approx(0.06));
}

TEST_CASE_FIXTURE(FfbFixture, "filters follow a changed update rate") {
	uint8_t cf = host.allocateEffect(FFB_EFFECT_CONSTANT);
	ffb.updateSamplerate(2000);
	CHECK(effect(cf).filter[0]->getFc() == doctest::Approx(0.25));
}

TEST_CASE_FIXTURE(FfbFixture, "unknown command ids are not found") {
	CHECK(fxCommand(static_cast<EffectsCalculator_commands>(0x7777), CMDtype::get) == CommandStatus::NOT_FOUND);
}

// ---------------------------------------------------------------------------
// Effect statistics
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(FfbFixture, "used effect types are recorded") {
	std::vector<CommandReply> replies;
	fxCommand(EffectsCalculator_commands::effects, CMDtype::info, 0, &replies);
	REQUIRE(replies.size() == 1);
	CHECK(replies[0].reply == "None");

	uint8_t cf = host.allocateEffect(FFB_EFFECT_CONSTANT);
	uint8_t spring = host.allocateEffect(FFB_EFFECT_SPRING);

	replies.clear();
	fxCommand(EffectsCalculator_commands::effects, CMDtype::get, 0, &replies);
	fxCommand(EffectsCalculator_commands::effects, CMDtype::info, 0, &replies);
	REQUIRE(replies.size() == 2);
	CHECK(replies[0].val == ((1 << (FFB_EFFECT_CONSTANT - 1)) | (1 << (FFB_EFFECT_SPRING - 1))));
	CHECK(replies[1].reply == "Constant,Spring");

	// Freed effects stay in the list until it is reset
	host.blockFree(cf);
	host.blockFree(spring);
	replies.clear();
	fxCommand(EffectsCalculator_commands::effects, CMDtype::get, 0, &replies);
	CHECK(replies[0].val != 0);
	fxCommand(EffectsCalculator_commands::effects, CMDtype::set, 0);
	replies.clear();
	fxCommand(EffectsCalculator_commands::effects, CMDtype::get, 0, &replies);
	CHECK(replies[0].val == 0);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "current forces per effect type can be read") {
	uint8_t second = startEffect(FFB_EFFECT_CONSTANT);
	host.setConstantForce(second, 2000);
	calculate();

	std::vector<CommandReply> replies;
	fxCommand(EffectsCalculator_commands::effectsForces, CMDtype::get, 0, &replies);
	REQUIRE(replies.size() == 12); // One entry per effect type
	CHECK(replies[FFB_EFFECT_CONSTANT - 1].val == -12000); // Sum of both effects on the X axis
	CHECK(replies[FFB_EFFECT_CONSTANT - 1].adr == 2);	   // Amount of effects of this type
	CHECK(replies[FFB_EFFECT_SPRING - 1].val == 0);
	CHECK(replies[FFB_EFFECT_SPRING - 1].adr == 0);

	replies.clear();
	fxCommand(EffectsCalculator_commands::effectsForces, CMDtype::getat, 0, &replies, 1); // Y axis
	REQUIRE(replies.size() == 12);
	CHECK(replies[FFB_EFFECT_CONSTANT - 1].val == 0);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "effect details list maximum and current force as json fragments") {
	calculate();
	std::vector<CommandReply> replies;
	fxCommand(EffectsCalculator_commands::effectsDetails, CMDtype::get, 0, &replies);
	REQUIRE(replies.size() == 1);
	CHECK(replies[0].reply.rfind("{\"max\":10000, \"curr\":-10000, \"nb\":1}, {\"max\":0, \"curr\":0, \"nb\":0}", 0) == 0);
}

TEST_CASE_FIXTURE(ConstantForceFixture, "requesting the forces of a not existing axis is rejected" *
		KNOWN_ISSUE_UB("effectsForces limits the axis to MAX_AXIS instead of MAX_AXIS-1 and reads current[MAX_AXIS] out of bounds")) {
	calculate();
	std::vector<CommandReply> replies;
	fxCommand(EffectsCalculator_commands::effectsForces, CMDtype::getat, 0, &replies, MAX_AXIS);
	// The out of bounds read returns the max value of the X axis which follows the current values in memory
	REQUIRE(replies.size() == 12);
	CHECK(replies[FFB_EFFECT_CONSTANT - 1].val == 0);
}

// ---------------------------------------------------------------------------
// Persistent settings
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(FfbFixture, "settings are saved to and restored from flash") {
	fxCommand(EffectsCalculator_commands::spring, CMDtype::set, 11);
	fxCommand(EffectsCalculator_commands::damper, CMDtype::set, 22);
	fxCommand(EffectsCalculator_commands::friction, CMDtype::set, 33);
	fxCommand(EffectsCalculator_commands::inertia, CMDtype::set, 44);
	fxCommand(EffectsCalculator_commands::ffbfiltercf, CMDtype::set, 123);
	fxCommand(EffectsCalculator_commands::ffbfiltercf_q, CMDtype::set, 45);
	fxCommand(EffectsCalculator_commands::frictionPctSpeedToRampup, CMDtype::set, 66);
	fxCommand(EffectsCalculator_commands::reconFilterMode, CMDtype::set, 2);
	fxCommand(EffectsCalculator_commands::filterProfileId, CMDtype::set, CUSTOM_PROFILE_ID);
	fxCommand(EffectsCalculator_commands::damper_f, CMDtype::set, 77);
	fxCommand(EffectsCalculator_commands::damper_q, CMDtype::set, 55);
	fxCommand(EffectsCalculator_commands::friction_f, CMDtype::set, 88);
	fxCommand(EffectsCalculator_commands::inertia_f, CMDtype::set, 99);
	ec->saveFlash();
	CHECK_FALSE(HostPlatform::flash().empty());

	// A new instance reads the stored values like after a reboot
	EffectsCalculator restored;
	auto get = [&](EffectsCalculator_commands cmd) {
		std::vector<CommandReply> replies;
		ParsedCommand parsed;
		parsed.cmdId = static_cast<uint32_t>(cmd);
		parsed.type = CMDtype::get;
		restored.command(parsed, replies);
		REQUIRE(replies.size() == 1);
		return replies[0].val;
	};
	CHECK(get(EffectsCalculator_commands::spring) == 11);
	CHECK(get(EffectsCalculator_commands::damper) == 22);
	CHECK(get(EffectsCalculator_commands::friction) == 33);
	CHECK(get(EffectsCalculator_commands::inertia) == 44);
	CHECK(get(EffectsCalculator_commands::ffbfiltercf) == 123);
	CHECK(get(EffectsCalculator_commands::ffbfiltercf_q) == 45);
	CHECK(get(EffectsCalculator_commands::frictionPctSpeedToRampup) == 66);
	CHECK(get(EffectsCalculator_commands::reconFilterMode) == 2);
	CHECK(get(EffectsCalculator_commands::filterProfileId) == CUSTOM_PROFILE_ID);
	CHECK(get(EffectsCalculator_commands::damper_f) == 77);
	CHECK(get(EffectsCalculator_commands::damper_q) == 55);
	CHECK(get(EffectsCalculator_commands::friction_f) == 88);
	CHECK(get(EffectsCalculator_commands::inertia_f) == 99);
}

TEST_CASE_FIXTURE(FfbFixture, "defaults are used with empty flash") {
	EffectsCalculator fresh;
	CHECK(fresh.getGain() == 255);
	CHECK_FALSE(fresh.isActive());
}

// ---------------------------------------------------------------------------
// Effect storage
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(FfbFixture, "find free effect returns the first unused slot") {
	CHECK(ec->find_free_effect(FFB_EFFECT_CONSTANT) == 0);
	ec->effects[0].type = FFB_EFFECT_CONSTANT;
	ec->effects[1].type = FFB_EFFECT_SINE;
	CHECK(ec->find_free_effect(FFB_EFFECT_CONSTANT) == 2);
	ec->free_effect(0);
	CHECK(ec->find_free_effect(FFB_EFFECT_CONSTANT) == 0);

	CHECK(ec->find_free_effect(FFB_EFFECT_NONE) == -1);
	CHECK(ec->find_free_effect(FFB_EFFECT_CUSTOM + 1) == -1);
	ec->free_effect(MAX_EFFECTS); // Out of range is ignored
}

} // TEST_SUITE
