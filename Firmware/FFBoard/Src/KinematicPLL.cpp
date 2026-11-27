#include "KinematicPLL.h"

#include <cmath>
#include <algorithm>

KinematicPLL::KinematicPLL() {
	setTuning(120.0f, 1.0f, 35.0f, 90.0f);
	reset(0.0f);
}

void KinematicPLL::reset(float initial_pos_rot) {
	pos_est = initial_pos_rot;
	vel_est = 0.0f;
	vel_ffb = 0.0f;
	vel_prev = 0.0f;
	integrator = 0.0f;
	accel_stage1 = 0.0f;
	accel_est = 0.0f;
	glitch_count = 0;
	initialized = false;
}

void KinematicPLL::reanchor(float new_pos_rot) {
	pos_est = new_pos_rot;
	glitch_count = 0;
	initialized = true;
}

void KinematicPLL::setTuning(float fn_hz, float damping, float accel_fc_hz, float vel_fc_hz) {
	fn_target = std::max(fn_hz, 1.0f);
	damping_target = std::max(damping, 0.1f);
	float wn = 2.0f * PI_F * fn_target;
	kp = 2.0f * damping_target * wn;
	ki = wn * wn;
	float effective_vel_fc = (vel_fc_hz > 0.0f) ? vel_fc_hz : (0.85f * fn_target);
	tau_vel = 1.0f / (2.0f * PI_F * std::max(effective_vel_fc, 1.0f));
	tau_accel = 1.0f / (2.0f * PI_F * std::max(accel_fc_hz, 1.0f));
}

void KinematicPLL::update(float z_pos_rot, float dt) {
	if (!initialized) {
		pos_est = z_pos_rot;
		vel_est = 0.0f;
		vel_ffb = 0.0f;
		vel_prev = 0.0f;
		integrator = 0.0f;
		accel_stage1 = 0.0f;
		accel_est = 0.0f;
		glitch_count = 0;
		initialized = true;
		return;
	}

	if (dt <= 1e-6f || dt > 0.05f) {
		// Reject invalid or stalled RTOS timesteps; re-anchor safely
		pos_est = z_pos_rot;
		vel_est = 0.0f;
		vel_ffb = 0.0f;
		vel_prev = 0.0f;
		integrator = 0.0f;
		accel_stage1 = 0.0f;
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
			vel_prev = vel_ffb;
			glitch_count = 0;
			return;
		}
		// Transient EMI/CRC glitch: coast down safely
		vel_est *= 0.95f;
		vel_ffb *= 0.95f;
		pos_est += vel_est * dt;
		accel_stage1 *= 0.90f;
		accel_est *= 0.90f;
		return;
	}
	glitch_count = 0;

	// 3. Type-II PI Velocity Observer with Euler stability bound (fn <= 0.05 * fs) and anti-windup clamping
	float eff_kp = kp;
	float eff_ki = ki;
	float max_stable_fn = 0.05f / dt; // Guarantees wn * dt <= 0.314 < 0.83 (explicit Euler stability bound)
	if (fn_target > max_stable_fn) {
		float wn = 2.0f * PI_F * max_stable_fn;
		eff_kp = 2.0f * damping_target * wn;
		eff_ki = wn * wn;
	}
	integrator = std::clamp(integrator + eff_ki * error * dt, -MAX_RPS, MAX_RPS);
	vel_est = std::clamp(eff_kp * error + integrator, -MAX_RPS, MAX_RPS);

	// 4. Closed-loop position integration (must use full PI vel_est for critical damping)
	pos_est += vel_est * dt;

	// 5. Decoupled 3rd-order FFB velocity from PLL integrator (eliminates kp * error quantization step)
	vel_prev = vel_ffb;
	float alpha_vel = dt / (tau_vel + dt);
	vel_ffb += alpha_vel * (integrator - vel_ffb);

	// 6. Decoupled 4th-order acceleration estimator from smooth FFB velocity slope
	// (vel_ffb - vel_prev) / dt == (integrator - vel_prev) / (tau_vel + dt), 3rd-order smooth without kp * error
	float accel_raw = (vel_ffb - vel_prev) / dt;
	float alpha_acc = dt / (tau_accel + dt);
	accel_stage1 += alpha_acc * (accel_raw - accel_stage1);
	accel_est = std::clamp(accel_stage1, -MAX_ACCEL, MAX_ACCEL);
}

void KinematicPLL::updateFromVelocity(float vel_rps, float dt) {
	float clamped_vel = std::clamp(vel_rps, -MAX_RPS, MAX_RPS);
	if (!initialized || dt <= 1e-6f || dt > 0.05f) {
		vel_prev = clamped_vel;
		vel_est = clamped_vel;
		vel_ffb = clamped_vel;
		integrator = clamped_vel;
		accel_stage1 = 0.0f;
		accel_est = 0.0f;
		initialized = true;
		return;
	}
	vel_est = clamped_vel;
	integrator = clamped_vel;
	vel_prev = vel_ffb;
	float alpha_vel = dt / (tau_vel + dt);
	vel_ffb += alpha_vel * (clamped_vel - vel_ffb);

	float accel_raw = (vel_ffb - vel_prev) / dt;
	float alpha_acc = dt / (tau_accel + dt);
	accel_stage1 += alpha_acc * (accel_raw - accel_stage1);
	accel_est = std::clamp(accel_stage1, -MAX_ACCEL, MAX_ACCEL);
}
