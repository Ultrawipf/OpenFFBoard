#include "KinematicPLL.h"

#include <cmath>
#include <algorithm>

KinematicPLL::KinematicPLL() {
	setTuning(120.0f, 1.0f, 60.0f);
	reset(0.0f);
}

void KinematicPLL::reset(float initial_pos_rot) {
	pos_est = initial_pos_rot;
	vel_est = 0.0f;
	vel_prev = 0.0f;
	integrator = 0.0f;
	accel_est = 0.0f;
	glitch_count = 0;
	initialized = false;
}

void KinematicPLL::reanchor(float new_pos_rot) {
	pos_est = new_pos_rot;
	glitch_count = 0;
	initialized = true;
}

void KinematicPLL::setTuning(float fn_hz, float damping, float accel_fc_hz) {
	float wn = 2.0f * PI_F * std::max(fn_hz, 1.0f);
	kp = 2.0f * damping * wn;
	ki = wn * wn;
	tau_accel = 1.0f / (2.0f * PI_F * std::max(accel_fc_hz, 1.0f));
}

void KinematicPLL::update(float z_pos_rot, float dt) {
	if (!initialized) {
		pos_est = z_pos_rot;
		vel_est = 0.0f;
		vel_prev = 0.0f;
		integrator = 0.0f;
		accel_est = 0.0f;
		glitch_count = 0;
		initialized = true;
		return;
	}

	if (dt <= 1e-6f || dt > 0.05f) {
		// Reject invalid or stalled RTOS timesteps; re-anchor safely
		pos_est = z_pos_rot;
		vel_est = 0.0f;
		vel_prev = 0.0f;
		integrator = 0.0f;
		accel_est = 0.0f;
		glitch_count = 0;
		return;
	}

	// 1. Continuous multi-turn tracking error
	float error = z_pos_rot - pos_est;

	// 2. Time-proportional Outlier Gating (3x physical max displacement over dt)
	float max_step_rot = std::max(MAX_RPS * dt * 3.0f, 0.004f);
	if (std::fabs(error) > max_step_rot) {
		if (++glitch_count > 3) {
			// Persistent step (e.g. explicit setPos() recenter): re-anchor without spike
			pos_est = z_pos_rot;
			vel_prev = vel_est;
			glitch_count = 0;
			return;
		}
		// Transient EMI/CRC glitch: coast down safely
		vel_est *= 0.95f;
		pos_est += vel_est * dt;
		accel_est *= 0.90f;
		return;
	}
	glitch_count = 0;

	// 3. Type-II PI Velocity Observer with anti-windup clamping
	integrator = std::clamp(integrator + ki * error * dt, -MAX_RPS, MAX_RPS);
	vel_prev = vel_est;
	vel_est = std::clamp(kp * error + integrator, -MAX_RPS, MAX_RPS);

	// 4. Closed-loop position integration
	pos_est += vel_est * dt;

	// 5. Decoupled 1st-order RC acceleration estimator (never fed back into pos/vel)
	float accel_raw = (vel_est - vel_prev) / dt;
	float alpha_rc = dt / (tau_accel + dt);
	accel_est = std::clamp(accel_est + alpha_rc * (accel_raw - accel_est), -MAX_ACCEL, MAX_ACCEL);
}

void KinematicPLL::updateFromVelocity(float vel_rps, float dt) {
	float clamped_vel = std::clamp(vel_rps, -MAX_RPS, MAX_RPS);
	if (!initialized || dt <= 1e-6f || dt > 0.05f) {
		vel_prev = clamped_vel;
		vel_est = clamped_vel;
		integrator = clamped_vel;
		accel_est = 0.0f;
		initialized = true;
		return;
	}
	vel_prev = vel_est;
	vel_est = clamped_vel;
	integrator = clamped_vel;
	float accel_raw = (vel_est - vel_prev) / dt;
	float alpha_rc = dt / (tau_accel + dt);
	accel_est = std::clamp(accel_est + alpha_rc * (accel_raw - accel_est), -MAX_ACCEL, MAX_ACCEL);
}
