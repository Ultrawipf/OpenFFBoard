#pragma once

#include <cstdint>

/**
 * @brief Industrial 2nd-order Tracking Loop (Type-II PLL) in continuous rotations.
 * Separates closed-loop PI tracking velocity (getRawSpeed(), used for FOC extrapolation)
 * from 3rd-order low-pass FFB velocity (getSpeed(), free of kp * error quantization steps)
 * and 4th-order critically-damped acceleration (getAccel()).
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
	 * @brief Tune PLL natural frequency and decoupled FFB velocity / acceleration filters.
	 * @param fn_hz Natural frequency (130 Hz for >=16-bit @>=4kHz, 65 Hz @1kHz, 45 Hz for low CPR, 30 Hz for CAN).
	 * @param damping Damping ratio (1.0f = critical damping, zero overshoot).
	 * @param accel_fc_hz Cutoff for the decoupled acceleration LPF stage (typ. 18-40 Hz).
	 * @param vel_fc_hz Cutoff for the FFB velocity post-integrator RC stage (0 = auto 0.85 * fn_hz).
	 */
	void setTuning(float fn_hz, float damping = 1.0f, float accel_fc_hz = 35.0f, float vel_fc_hz = 0.0f);

	/**
	 * @brief Update the PLL with a new continuous multi-turn position measurement.
	 * @param z_pos_rot Measured multi-turn angle in rotations (continuous, unwrapped).
	 * @param dt Time step in seconds (e.g. 0.0001f at 10 kHz, 0.001f at 1 kHz).
	 */
	void update(float z_pos_rot, float dt);

	/**
	 * @brief Direct velocity injection for external drivers providing calibrated speed (e.g. ODriveCAN).
	 * Updates FFB velocity and decoupled acceleration without running position loop.
	 */
	void updateFromVelocity(float vel_rps, float dt);

	float getPos() const      { return pos_est; }   // rotations
	float getRawSpeed() const { return vel_est; }   // rot/s (fast PI loop output for FOC extrapolation)
	float getSpeed() const    { return vel_ffb; }   // rot/s (smooth 3rd-order output for FFB Damper/Friction)
	float getAccel() const    { return accel_est; } // rot/s^2 (smooth 4th-order output for FFB Inertia)

private:
	float pos_est = 0.0f;
	float vel_est = 0.0f;
	float vel_ffb = 0.0f;
	float vel_prev = 0.0f;
	float integrator = 0.0f;
	float accel_stage1 = 0.0f;
	float accel_est = 0.0f;

	float kp = 1507.96f;
	float ki = 568489.0f;
	float tau_vel = 0.00176839f;   // ~90 Hz
	float tau_accel = 0.00454728f; // ~35 Hz
	uint8_t glitch_count = 0;
	bool initialized = false;
};
