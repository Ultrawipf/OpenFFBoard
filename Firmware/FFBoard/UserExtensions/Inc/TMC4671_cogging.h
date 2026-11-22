/*
 * TMC4671_cogging.h
 *
 * Dedicated calibrator class & data structures for TMC4671 anti-cogging & speed-ripple calibration.
 */

#ifndef TMC4671_COGGING_H_
#define TMC4671_COGGING_H_

#include <stdint.h>
#include <stddef.h>
#include "cppmain.h"

#ifdef COGGING_TABLE_FLASH_START_ADDRESS

class TMC4671;

// Step 1 output: True physical encoder resolution & dynamic PID decimation profile
struct CoggingEncoderProfile {
	uint32_t physical_cpr = 65536;
	uint32_t base_period_us = 250;
	uint32_t decimation_ratio = 1;
	float base_loop_hz = 4000.0f;
	float dt_decimated = 0.00025f;
	float calib_rpm = 3.0f;
	float resolution_penalty = 1.0f;
};

// 2-DOF Cascade Position + Active Velocity Damping + Conditional Anti-Windup Controller
// Stabilizes both heavy industrial servos and low-inertia small motors / steppers
struct CoggingCascadeController {
	float Kp = 0.0f;            // Position stiffness (Iq counts / turn)
	float Kv = 0.0f;            // Active velocity damping (Iq counts / (turn/s))
	float Ki = 0.0f;            // Continuous integral gain (Iq counts / (turn * s))
	float int_limit = 1500.0f;  // Anti-windup integrator clamp (Iq counts)
	float vel_alpha = 0.25f;    // EMA low-pass coefficient on velocity derivative
	float integrator = 0.0f;
	float vel_filt = 0.0f;

	void reset(float initial_vel_turns = 0.0f) {
		integrator = 0.0f;
		vel_filt = initial_vel_turns;
	}

	float update(float pos_err_turns, float target_vel_turns, float raw_vel_turns, float dt, float max_torque) {
		vel_filt += vel_alpha * (raw_vel_turns - vel_filt);
		float vel_err = target_vel_turns - vel_filt;
		float p_term = Kp * pos_err_turns;
		float v_term = Kv * vel_err;
		float pre_sat = p_term + v_term + integrator;
		// Conditional integration (Anti-windup): only integrate if not pushing further into saturation
		if ((pre_sat < max_torque && pre_sat > -max_torque) ||
			(pre_sat >= max_torque && pos_err_turns < 0.0f) ||
			(pre_sat <= -max_torque && pos_err_turns > 0.0f)) {
			integrator = clip<float, float>(integrator + (Ki * pos_err_turns * dt), -int_limit, int_limit);
		}
		return p_term + v_term + integrator;
	}
};

// Step 2 output: Physical system identification parameters & tuned cascade controller
struct CoggingSysIdResult {
	float max_test_torque = 2000.0f;
	float dynamic_friction = 0.0f; // Coulomb friction Tc (Iq counts)
	float B_phys = 0.0f;           // True viscous damping (Iq counts / (rad/s))
	float J_phys = 0.0f;           // True mechanical inertia (Iq counts / (rad/s^2))
	CoggingCascadeController ctrl;
};

// Heap-allocated DFT accumulators (separated FWD/BWD buffers eliminate cos(delta_phi) attenuation)
struct CoggingDftBuffers {
	float* iq_acc_cos = nullptr;
	float* iq_acc_sin = nullptr;
	float* iq_fwd_cos = nullptr;
	float* iq_fwd_sin = nullptr;
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
	float* id_acc_cos = nullptr;
	float* id_acc_sin = nullptr;
#endif
	uint32_t total_samples = 0;
	uint32_t fwd_samples = 0;
	uint32_t bwd_samples = 0;
	bool dual_dir_separated = false;
};

// Dedicated calibrator encapsulating the entire 8-stage cogging & multi-speed ripple pipeline
class TMC4671CoggingCalibrator {
public:
	explicit TMC4671CoggingCalibrator(TMC4671& driver) : tmc(driver) {}

	void run();

private:
	TMC4671& tmc;

	// Shared calibration utilities & immediate E-stop safety
	void broadcastLog(int val, const char* format, ...);
	bool isEstopActive();
	bool safeDelay(uint32_t ms);
	void waitNextTick(uint32_t next_tick);

	// 8 modular pipeline stages with explicit typed inputs/outputs
	void runSimulationDft(CoggingDftBuffers& buf);
	bool identifyEncoder(CoggingEncoderProfile& enc, const char** err);
	bool runSysId(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, const char** err);
	bool validateTracking(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, char* errBuf, size_t errBufSize, const char** err);
	bool acquireTier0Dft(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, CoggingDftBuffers& buf, const char** err);
	bool extractHarmonics(const CoggingDftBuffers& buf, int& main_h, uint16_t& max_order, const char** err);
	bool profileMultiSpeedRipple(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, int main_h, uint16_t max_order, const char** err);
	bool calibrateScale(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, int main_h, const char** err);
	bool returnToCenter(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, const char** err);
};

#endif // COGGING_TABLE_FLASH_START_ADDRESS
#endif /* TMC4671_COGGING_H_ */
