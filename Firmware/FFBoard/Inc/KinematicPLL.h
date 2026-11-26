#pragma once

#include <cstdint>

/**
 * @brief Industrial 2nd-order Tracking Loop (Type-II PLL) in continuous rotations.
 * Provides unconditionally stable multi-turn position (rot), velocity (rot/s),
 * and decoupled 1st-order RC-filtered acceleration (rot/s^2).
 */
class KinematicPLL {
public:
	static constexpr float PI_F      = 3.14159265f;
	static constexpr float MAX_RPS   = 11.14f;  // ~70 rad/s (~668 RPM)
	static constexpr float MAX_ACCEL = 318.3f;  // ~2000 rad/s^2

	KinematicPLL();

	void reset(float initial_pos_rot = 0.0f);

	/**
	 * @brief Re-anchor position estimate without injecting a velocity/acceleration spike
	 * (e.g. when user calls setPos(0) to recenter the wheel).
	 */
	void reanchor(float new_pos_rot);

	/**
	 * @brief Tune PLL natural frequency and decoupled acceleration filter.
	 * @param fn_hz Natural frequency (120-150 Hz for >=16-bit @10kHz, 60-80 Hz for low CPR / 1kHz).
	 * @param damping Damping ratio (1.0f = critical damping, zero overshoot).
	 * @param accel_fc_hz 1st-order LPF cutoff for acceleration output (typ. 60 Hz).
	 */
	void setTuning(float fn_hz, float damping = 1.0f, float accel_fc_hz = 60.0f);

	/**
	 * @brief Update the PLL with a new continuous multi-turn position measurement.
	 * @param z_pos_rot Measured multi-turn angle in rotations (continuous, unwrapped).
	 * @param dt Time step in seconds (e.g. 0.0001f at 10 kHz, 0.001f at 1 kHz).
	 */
	void update(float z_pos_rot, float dt);

	/**
	 * @brief Direct velocity injection for external drivers providing calibrated speed (e.g. ODriveCAN).
	 * Updates decoupled acceleration from velocity delta without running position loop.
	 */
	void updateFromVelocity(float vel_rps, float dt);

	float getPos() const   { return pos_est; }   // rotations
	float getSpeed() const { return vel_est; }   // rot/s
	float getAccel() const { return accel_est; } // rot/s^2

private:
	float pos_est = 0.0f;
	float vel_est = 0.0f;
	float vel_prev = 0.0f;
	float integrator = 0.0f;
	float accel_est = 0.0f;

	float kp = 1507.96f;
	float ki = 568489.0f;
	float tau_accel = 0.00265258f; // ~60 Hz
	uint8_t glitch_count = 0;
	bool initialized = false;
};
