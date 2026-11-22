/*
 * TMC4671_cogging.cpp
 *
 * Dedicated TMC4671CoggingCalibrator class implementing the 8-stage anti-cogging &
 * multi-speed ripple calibration pipeline with immediate hardware E-stop protection.
 */

#include "TMC4671.h"
#include "TMC4671_cogging.h"
#include "voltagesense.h"
#if defined(TMC4671DRIVER) && defined(COGGING_TABLE_FLASH_START_ADDRESS)

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace {
	constexpr uint32_t REVOLUTION_TIME_MS = COGGING_CALIB_TIME_PER_REV_S * 1500;
	constexpr uint32_t BREAKOUT_STEP_MS = 50;
	constexpr uint32_t SYSID_J_PULSE_MS = 120;
	constexpr uint32_t VAL_TOTAL_DURATION_MS = 2500;
	constexpr uint32_t COGGING_WARMUP_MS = 1500;
}

// ============================================================================
// TMC4671 Real-Time Position / Safe Torque Helpers (used by turn() & calibrator)
// ============================================================================

void TMC4671::applySafeTorque(float torque_cmd) {
	// Immediate hardware cut-off if E-stop is engaged
	if (emergency || estopTriggered) {
		enablePin.reset();
		setFluxTorque(0, 0);
		return;
	}
	float totalPower = torque_cmd;

	// Respect inversion settings
	if ((this->conf.encoderReversed && conf.motconf.enctype == EncoderType_TMC::ext) ^ conf.invertForce) {
		totalPower = -totalPower;
	}

	int16_t pwr = (int16_t)clip<float, float>(totalPower, -32768, 32767);
	// Use zero flux (Id = 0) to ensure measured current is 100% dedicated to torque (Iq)
	setFluxTorque(0, pwr);
}

float TMC4671::getWrappedError(float target, float actual) {
	target = target - floorf(target);
	actual = actual - floorf(actual);
	float err = target - actual;
	if (err > 0.5f) err -= 1.0f;
	if (err < -0.5f) err += 1.0f;
	return err;
}

float TMC4671::getAbsolutePosition() {
	Encoder* activeEnc = usingExternalEncoder() ? drvEncoder.get() : static_cast<Encoder*>(this);
	return activeEnc->getPos_f();
}

float TMC4671::getFilteredPosition() {
	Encoder* activeEnc = usingExternalEncoder() ? drvEncoder.get() : static_cast<Encoder*>(this);

	int32_t cpr = activeEnc->getCpr();
	if (cpr == 0) return 0.0f;

	// Integer modulo BEFORE float conversion guarantees zero precision loss across unlimited turns
	int32_t remainder = activeEnc->getPos() % cpr;
	if (remainder < 0) remainder += cpr;

	return (float)remainder / (float)cpr;
}

void TMC4671::handleStateCoggingCalibration() {
	TMC4671CoggingCalibrator calibrator(*this);
	calibrator.run();
}

// ============================================================================
// TMC4671CoggingCalibrator: Shared Utilities & Immediate E-Stop Guards
// ============================================================================

void TMC4671CoggingCalibrator::broadcastLog(int val, const char* format, ...) {
	char buf[128];
	buf[0] = '(';
	buf[1] = '\"';
	va_list args;
	va_start(args, format);
	int len = vsnprintf(buf + 2, sizeof(buf) - 16, format, args);
	va_end(args);
	if (len < 0) return;
	snprintf(buf + 2 + len, sizeof(buf) - len - 2, "\",%d)", val);
	tmc.broadcastCommandReply(CommandReply(std::string(buf)), (uint32_t)TMC4671::TMC4671_commands::calibrateCogging, CMDtype::get);
}

bool TMC4671CoggingCalibrator::isEstopActive() {
	if (tmc.isEmergencyActive()) return true;
	// Immediately abort calibration if brake resistor over-on fault occurred.
	if (brake_failure) {
		tmc.emergencyStop(false);
		return true;
	}
#ifdef E_STOP_Pin
	if (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
		tmc.emergency = true;
		tmc.motorEnabledRequested = false;
		tmc.enablePin.reset();
		tmc.setFluxTorque(0, 0);
		tmc.setPwm(TMC_PwmMode::off);
		return true;
	}
#endif
	return false;
}

bool TMC4671CoggingCalibrator::safeDelay(uint32_t ms) {
	uint32_t start = HAL_GetTick();
	while ((HAL_GetTick() - start) < ms) {
		if (isEstopActive()) return false;
		refreshWatchdog();
		tmc.Delay(5);
	}
	return !isEstopActive();
}

void TMC4671CoggingCalibrator::waitNextTick(uint32_t next_tick) {
#ifdef TIM_CALIBRATION
	if (tmc.calibTimer != nullptr) {
		tmc.WaitForNotification();
		return;
	}
#endif
	while ((micros() - next_tick) & 0x80000000) {}
}

// ============================================================================
// Step 0 (Offline Debug): Simulation DFT when unpowered
// ============================================================================

void TMC4671CoggingCalibrator::runSimulationDft(CoggingDftBuffers& buf) {
	broadcastLog(0, "Simulating acquisition (No power detected)...");
	buf.total_samples = 2000;
	buf.dual_dir_separated = false;
	for (uint32_t i = 0; i < buf.total_samples; i++) {
		float pos_f = (float)i / (float)buf.total_samples;
		float angle_rad = pos_f * 2.0f * PI;
		float iq = 1500.0f * arm_sin_f32(angle_rad * 13.0f) + 800.0f * arm_sin_f32(angle_rad * 36.0f + (PI / 2.0f));
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
		float id = 100.0f;
#endif
		float s1, c1;
		arm_sin_cos_f32(pos_f * 360.0f, &s1, &c1);

		float cur_s = s1, cur_c = c1;
		for (int k = 1; k < COGGING_CALIB_DFT_HARMONICS; k++) {
			buf.iq_acc_cos[k] += (iq * cur_c);
			buf.iq_acc_sin[k] += (iq * cur_s);
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
			buf.id_acc_cos[k] += (id * cur_c);
			buf.id_acc_sin[k] += (id * cur_s);
#endif
			float next_c = cur_c * c1 - cur_s * s1;
			float next_s = cur_c * s1 + cur_s * c1;
			cur_c = next_c;
			cur_s = next_s;
		}
		if (i % 100 == 0) refreshWatchdog();
	}
}

// ============================================================================
// Step 1/4: Encoder Physical Resolution Identification & Dynamic Decimation
// ============================================================================

bool TMC4671CoggingCalibrator::identifyEncoder(CoggingEncoderProfile& enc, const char** err) {
	if (isEstopActive()) {
		*err = "Abort: Emergency stop engaged";
		return false;
	}

	uint32_t enc_cpr = tmc.getEncCpr();
	const char* enc_type_str = "External";
	if (!tmc.usingExternalEncoder()) {
		if (tmc.conf.motconf.enctype == EncoderType_TMC::abn) {
			enc_cpr = tmc.abnconf.cpr;
			enc_type_str = "Internal ABN";
		} else if (tmc.conf.motconf.enctype == EncoderType_TMC::sincos || tmc.conf.motconf.enctype == EncoderType_TMC::uvw) {
			enc_cpr = clip<uint32_t, uint32_t>(tmc.aencconf.cpr, 1, 16) * 4096;
			enc_type_str = "Internal SinCos/UVW";
		} else {
			enc_cpr = tmc.getCpr();
			enc_type_str = "Internal Other";
		}
	}

	broadcastLog(0, "STEP 1/4: Encoder Identification (%s, physical CPR: %lu)...", enc_type_str, enc_cpr);

	enc.physical_cpr = enc_cpr;
	enc.base_period_us = std::max<uint32_t>(1, tmc.getActualCalibPeriod(TIM_TMC_ARR));
	enc.base_loop_hz = 1000000.0f / (float)enc.base_period_us;
	enc.calib_rpm = 60.0f / (float)COGGING_CALIB_TIME_PER_REV_S;

	const char* enc_perf = "High-Res";
	if (enc_cpr > 0 && enc_cpr < 16384) {
		enc.calib_rpm = (enc_cpr < 8192) ? 10.0f : 6.0f;
		enc_perf = "Low-Res";
	} else if (enc_cpr >= 16384 && enc_cpr < 50000) {
		enc_perf = "Medium-Res";
	}

	// Guarantee >= 4 physical encoder ticks per PID/acceleration step
	float ticks_per_sec = (float)enc_cpr * (enc.calib_rpm / 60.0f);
	if (ticks_per_sec > 1.0f) {
		uint32_t needed_decim = (uint32_t)ceilf((4.0f * enc.base_loop_hz) / ticks_per_sec);
		enc.decimation_ratio = clip<uint32_t, uint32_t>(needed_decim, 1, 16);
	} else {
		enc.decimation_ratio = 16;
	}

	float pid_hz = enc.base_loop_hz / (float)enc.decimation_ratio;
	float ticks_per_pid_step = ticks_per_sec / pid_hz;
	enc.dt_decimated = ((float)enc.base_period_us / 1000000.0f) * (float)enc.decimation_ratio;
	enc.resolution_penalty = clip<float, float>(1.0f / sqrtf((float)enc.decimation_ratio), 0.25f, 1.0f);

	broadcastLog(0, "Enc Class: %s | Loop: %.0fHz / %lu = %.0fHz | RPM: %.1f (%.1f ticks/step, Pen: %.2f)",
		enc_perf, enc.base_loop_hz, enc.decimation_ratio, pid_hz, enc.calib_rpm, ticks_per_pid_step, enc.resolution_penalty);
	return true;
}

// ============================================================================
// Step 2/4: Physical System Identification (Breakout, 2-Speed Friction, Inertia)
// ============================================================================

bool TMC4671CoggingCalibrator::runSysId(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, const char** err) {
	broadcastLog(0, "STEP 2/4: Physical System Identification (2-Speed Friction + Dual-Dir Inertia)...");

	sysid.max_test_torque = tmc.bangInitPower > 0 ? (float)tmc.bangInitPower * 0.8f : 2000.0f;
	float starting_torque = tmc.bangInitPower > 0 ? clip<float, float>((float)tmc.bangInitPower * 0.05f, 50.0f, 250.0f) : 100.0f;

	if (tmc.coggingSpeedP != 0.0f || tmc.coggingSpeedI != 0.0f) {
		sysid.ctrl.Kp = tmc.coggingSpeedP;
		sysid.ctrl.Kv = 0.0f;
		sysid.ctrl.Ki = tmc.coggingSpeedI;
		sysid.ctrl.int_limit = sysid.max_test_torque * 0.6f;
		sysid.ctrl.vel_alpha = 0.25f;
		broadcastLog(0, "Manual PID Override -> Kp:%.0f Ki:%.0f (Kv:0, maxT:%.0f)",
			sysid.ctrl.Kp, sysid.ctrl.Ki, sysid.max_test_torque);
		return true;
	}

	// --- Step 2.1: Fine static friction (Breakout) ---
	float start_pos = tmc.getAbsolutePosition();
	float tuning_torque = starting_torque;
	bool friction_broken = false;
	broadcastLog(0, "Step 2.1: Finding breakout torque (start: %.0f, max: %.0f)...", starting_torque, sysid.max_test_torque);
	while (!friction_broken && tuning_torque < sysid.max_test_torque && !isEstopActive() && tmc.hasPower()) {
		tmc.applySafeTorque(tuning_torque);
		if (!safeDelay(BREAKOUT_STEP_MS)) { *err = "Abort: Emergency stop engaged"; return false; }
		if (fabsf(tmc.getWrappedError(start_pos, tmc.getAbsolutePosition())) > 0.005f) {
			friction_broken = true;
		} else {
			tuning_torque += (tuning_torque < 500.0f) ? 50.0f : 100.0f;
		}
		refreshWatchdog();
	}
	tmc.applySafeTorque(0);
	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }
	if (!friction_broken) { *err = "Abort: Motor failed to break static friction"; return false; }

	float breakout_torque = tuning_torque;
	broadcastLog(0, "Breakout torque detected: %.0f Iq counts", breakout_torque);
	if (!safeDelay(300)) { *err = "Abort: Emergency stop engaged"; return false; }

	// --- Step 2.2: Two-speed friction separation (15 vs 45 RPM over the exact same 90 deg sector) ---
	broadcastLog(0, "Step 2.2: Separating Coulomb & Viscous friction (15 vs 45 RPM, same 90 deg sector)...");
	float sector_start = tmc.getAbsolutePosition();
	constexpr float sector_span = 0.25f; // 90 deg
	float kp_sector = clip<float, float>(breakout_torque * 12.0f, 1500.0f, 30000.0f);
	float mean_trq_speeds[2] = {0.0f, 0.0f};
	constexpr float test_rpms[2] = {15.0f, 45.0f};
	float dt_sec = (float)enc.base_period_us / 1000000.0f;

	for (int s_idx = 0; s_idx < 2 && !isEstopActive() && tmc.hasPower(); s_idx++) {
		float w_turns = test_rpms[s_idx] / 60.0f;
		float b_pos = tmc.getAbsolutePosition();
		float b_sum_torque = 0.0f;
		uint32_t b_samples = 0;
		uint32_t b_start = HAL_GetTick();
		uint32_t b_timeout = (s_idx == 0) ? 3500 : 2000;
		uint32_t next_tick = micros();
		bool entered_sector = false;

		tmc.startCalibTimers(TIM_TMC_ARR);
		while ((HAL_GetTick() - b_start) < b_timeout && !isEstopActive() && tmc.hasPower()) {
			next_tick += enc.base_period_us;
			b_pos += w_turns * dt_sec;
			float cur_pos = tmc.getAbsolutePosition();
			float cmd = clip<float, float>((b_pos - cur_pos) * kp_sector + breakout_torque * 0.6f, -sysid.max_test_torque, sysid.max_test_torque);
			tmc.applySafeTorque(cmd);

			float rel_pos = cur_pos - sector_start;
			if (rel_pos >= 0.05f && rel_pos <= (0.05f + sector_span)) {
				entered_sector = true;
				b_sum_torque += cmd;
				b_samples++;
			} else if (entered_sector && rel_pos > (0.05f + sector_span)) {
				break;
			}
			refreshWatchdog();
			waitNextTick(next_tick);
		}
		tmc.stopCalibTimers();
		tmc.applySafeTorque(0);
		if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

		mean_trq_speeds[s_idx] = (b_samples > 0) ? (b_sum_torque / (float)b_samples) : breakout_torque;

		// Return to sector_start before second speed measurement so both cover identical angles
		if (s_idx == 0 && !isEstopActive() && tmc.hasPower()) {
			if (!safeDelay(150)) { *err = "Abort: Emergency stop engaged"; return false; }
			uint32_t ret_start = HAL_GetTick();
			float ret_pos = tmc.getAbsolutePosition();
			next_tick = micros();
			tmc.startCalibTimers(TIM_TMC_ARR);
			while (fabsf(tmc.getAbsolutePosition() - sector_start) > 0.005f && (HAL_GetTick() - ret_start) < 2500 && !isEstopActive() && tmc.hasPower()) {
				next_tick += enc.base_period_us;
				ret_pos += (ret_pos > sector_start) ? (-0.35f * dt_sec) : (0.35f * dt_sec);
				float cmd = clip<float, float>((ret_pos - tmc.getAbsolutePosition()) * kp_sector, -sysid.max_test_torque * 0.6f, sysid.max_test_torque * 0.6f);
				tmc.applySafeTorque(cmd);
				refreshWatchdog();
				waitNextTick(next_tick);
			}
			tmc.stopCalibTimers();
			tmc.applySafeTorque(0);
			if (!safeDelay(200)) { *err = "Abort: Emergency stop engaged"; return false; }
		}
	}
	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

	float w1_rad = (test_rpms[0] / 60.0f) * 2.0f * PI;
	float w2_rad = (test_rpms[1] / 60.0f) * 2.0f * PI;
	sysid.B_phys = clip<float, float>((mean_trq_speeds[1] - mean_trq_speeds[0]) / (w2_rad - w1_rad), 0.0f, 1000.0f);
	sysid.dynamic_friction = clip<float, float>(mean_trq_speeds[0] - (sysid.B_phys * w1_rad), 20.0f, sysid.max_test_torque * 0.7f);
	float B_scaled = sysid.B_phys * 100.0f;

	broadcastLog(0, "Friction ID -> T@15:%.0f T@45:%.0f => Tc:%.0f B_phys:%.2f",
		mean_trq_speeds[0], mean_trq_speeds[1], sysid.dynamic_friction, sysid.B_phys);
	if (!safeDelay(300)) { *err = "Abort: Emergency stop engaged"; return false; }

	// --- Step 2.3: Dual-direction Inertia J corrected for Coulomb friction Tc ---
	broadcastLog(0, "Step 2.3: Measuring Inertia J (dual-dir pulse, net torque = T_pulse - Tc)...");
	float j_torque_min = std::min<float>(sysid.dynamic_friction + 300.0f, sysid.max_test_torque * 0.75f);
	float j_torque = clip<float, float>(breakout_torque * 2.2f, j_torque_min, sysid.max_test_torque * 0.75f);
	float net_j_torque = std::max<float>(100.0f, j_torque - sysid.dynamic_friction);
	float j_sum = 0.0f;
	int j_valid_cnt = 0;

	for (int dir = 0; dir < 2 && !isEstopActive() && tmc.hasPower(); dir++) {
		float sign = (dir == 0) ? 1.0f : -1.0f;
		start_pos = tmc.getAbsolutePosition();
		uint32_t j_start_us = micros();
		uint32_t j_target_us = j_start_us + (SYSID_J_PULSE_MS * 1000);

		tmc.applySafeTorque(sign * j_torque);
		while (((micros() - j_target_us) & 0x80000000) && !isEstopActive() && tmc.hasPower()) {
			if (fabsf(tmc.getAbsolutePosition() - start_pos) > 0.20f) break;
			refreshWatchdog();
		}
		uint32_t j_end_us = micros();
		float end_pos = tmc.getAbsolutePosition();
		tmc.applySafeTorque(0);
		if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

		float dt_j = (float)(j_end_us - j_start_us) / 1000000.0f;
		float d_pos_rad = fabsf(end_pos - start_pos) * 2.0f * PI;
		if (d_pos_rad > 0.005f && dt_j > 0.005f) {
			j_sum += (net_j_torque * dt_j * dt_j) / (2.0f * d_pos_rad);
			j_valid_cnt++;
		}
		if (!safeDelay(350)) { *err = "Abort: Emergency stop engaged"; return false; }
	}
	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }
	if (j_valid_cnt == 0) { *err = "Abort: Motor did not move during inertia measurement"; return false; }

	sysid.J_phys = j_sum / (float)j_valid_cnt;
	float J_scaled = sysid.J_phys * 100.0f;

	// --- Step 2.4: Cascade Pole Placement (Kp, Kv, Ki) ---
	float J_turns = 2.0f * PI * sysid.J_phys;
	float B_turns = 2.0f * PI * sysid.B_phys;
	float f_bw = clip<float>(16.5f - (0.0047f * J_scaled), 6.0f, 15.0f);
	float wn = 2.0f * PI * f_bw;

	float kv_discrete_limit = (enc.dt_decimated > 0.00005f) ? (0.35f * J_turns / enc.dt_decimated) : 50000.0f;
	sysid.ctrl.Kp = clip<float, float>(1.6f * wn * wn * J_turns * enc.resolution_penalty, 250.0f, 250000.0f);
	sysid.ctrl.Kv = clip<float, float>((2.2f * wn * J_turns - B_turns) * enc.resolution_penalty, 0.0f, kv_discrete_limit);
	sysid.ctrl.Ki = clip<float, float>(0.35f * wn * wn * wn * J_turns * enc.resolution_penalty * enc.resolution_penalty, 10.0f, 200000.0f);
	sysid.ctrl.int_limit = sysid.max_test_torque * 0.6f;

	float fc_vel = 3.0f * f_bw;
	sysid.ctrl.vel_alpha = clip<float, float>((2.0f * PI * fc_vel * enc.dt_decimated) / (1.0f + 2.0f * PI * fc_vel * enc.dt_decimated), 0.05f, 0.8f);

	broadcastLog(0, "SysID -> J:%.1f B:%.1f f_bw:%.1fHz | Cascade Kp:%.0f Kv:%.1f Ki:%.0f",
		J_scaled, B_scaled, f_bw, sysid.ctrl.Kp, sysid.ctrl.Kv, sysid.ctrl.Ki);
	return true;
}

// ============================================================================
// Step 2.5: Stability Validation Rotation
// ============================================================================

bool TMC4671CoggingCalibrator::validateTracking(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, char* errBuf, size_t errBufSize, const char** err) {
	broadcastLog(0, "Step 2.5: Stability validation rotation at %.1f RPM...", enc.calib_rpm);

	float val_target_pos = tmc.getAbsolutePosition();
	float val_prev_pos = val_target_pos;
	float max_err_deg = 0.0f;
	uint32_t val_start = HAL_GetTick();
	uint32_t next_tick = micros();
	float dt_sec = (float)enc.base_period_us / 1000000.0f;
	float val_target_vel = enc.calib_rpm / 60.0f;
	uint32_t dec_cnt = 0;

	sysid.ctrl.reset(val_target_vel);
	tmc.startCalibTimers(TIM_TMC_ARR);
	while (HAL_GetTick() - val_start < VAL_TOTAL_DURATION_MS && !isEstopActive() && tmc.hasPower()) {
		next_tick += enc.base_period_us;
		val_target_pos += val_target_vel * dt_sec;

		if (dec_cnt % enc.decimation_ratio == 0) {
			float cur_pos = tmc.getAbsolutePosition();
			float pos_err = val_target_pos - cur_pos;
			float raw_vel = (cur_pos - val_prev_pos) / enc.dt_decimated;
			val_prev_pos = cur_pos;

			float iq_pid = sysid.ctrl.update(pos_err, val_target_vel, raw_vel, enc.dt_decimated, sysid.max_test_torque);
			float iq_ff = sysid.dynamic_friction + (sysid.B_phys * 2.0f * PI * val_target_vel);
			tmc.applySafeTorque(clip<float, float>(iq_pid + iq_ff, -sysid.max_test_torque, sysid.max_test_torque));

			if (HAL_GetTick() - val_start > COGGING_WARMUP_MS) {
				if (fabsf(pos_err) * 360.0f > max_err_deg) max_err_deg = fabsf(pos_err) * 360.0f;
			}
		}
		dec_cnt++;
		refreshWatchdog();
		waitNextTick(next_tick);
	}
	tmc.stopCalibTimers();
	tmc.applySafeTorque(0);

	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }
	if (max_err_deg > 5.0f) {
		snprintf(errBuf, errBufSize, "Abort: Stability check failed (%.2f deg > 5.0 deg)", max_err_deg);
		broadcastLog(0, "%s", errBuf);
		*err = errBuf;
		return false;
	}
	broadcastLog(0, "Validation OK (Max tracking error: %.2f deg <= 5.0 deg)", max_err_deg);
	if (!safeDelay(250)) { *err = "Abort: Emergency stop engaged"; return false; }
	return true;
}

// ============================================================================
// Step 3/4: Tier 0 Static Cogging DFT (Separated FWD/BWD + Causal Inertia)
// ============================================================================

bool TMC4671CoggingCalibrator::acquireTier0Dft(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, CoggingDftBuffers& buf, const char** err) {
	broadcastLog(0, "STEP 3/4: Tier 0 Static DFT at %.1f RPM (FWD+BWD separated, causal inertia)...", enc.calib_rpm);
	const int8_t dirs[2] = {1, -1};
	buf.dual_dir_separated = true;
	float dt_sec = (float)enc.base_period_us / 1000000.0f;
	float iq_ema_alpha = clip<float, float>((2.0f * PI * 150.0f * enc.dt_decimated) / (1.0f + 2.0f * PI * 150.0f * enc.dt_decimated), 0.1f, 0.9f);

	for (int8_t p : dirs) {
		if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }
		broadcastLog(0, "Tier 0 %s pass at %.1f RPM (1.0 rev)...", (p == 1) ? "FWD" : "BWD", enc.calib_rpm);

		float target_rpm = (p == 1) ? enc.calib_rpm : -enc.calib_rpm;
		float target_vel_turns = target_rpm / 60.0f;
		float target_pos_f = tmc.getFilteredPosition();
		float integrated_distance = 0.0f;
		float max_iq_cmd_used = 0.0f;
		float max_err_seen = 0.0f;
		float* cur_cos_acc = (p == 1) ? buf.iq_fwd_cos : buf.iq_acc_cos;
		float* cur_sin_acc = (p == 1) ? buf.iq_fwd_sin : buf.iq_acc_sin;

		sysid.ctrl.reset(target_vel_turns);
		if (!safeDelay(1000)) { *err = "Abort: Emergency stop engaged"; return false; }

		if (!isEstopActive() && tmc.hasPower()) {
			tmc.calibStartTime = HAL_GetTick();
			uint32_t next_tick = micros();
			float prev_actual_pos_f = tmc.getFilteredPosition();
			float prev_vel_turns = target_vel_turns;
			uint32_t dec_cnt = 0;
			float iq_cmd = 0.0f;
			float prev_iq_cmd = 0.0f;
			float iq_mag_filt = 0.0f;

			tmc.startCalibTimers(TIM_TMC_ARR);
			while (HAL_GetTick() - tmc.calibStartTime < REVOLUTION_TIME_MS && !isEstopActive() && tmc.hasPower()) {
				next_tick += enc.base_period_us;

				float step = target_vel_turns * dt_sec;
				target_pos_f += step;
				if (target_pos_f >= 1.0f) target_pos_f -= 1.0f;
				if (target_pos_f < 0.0f) target_pos_f += 1.0f;

				float actual_pos_f = tmc.getFilteredPosition();

				if (dec_cnt % enc.decimation_ratio == 0) {
					float error = tmc.getWrappedError(target_pos_f, actual_pos_f);
					float delta_pos = tmc.getWrappedError(prev_actual_pos_f, actual_pos_f);
					float current_vel_turns = delta_pos / enc.dt_decimated;
					float current_accel_rad = ((current_vel_turns - prev_vel_turns) / enc.dt_decimated) * 2.0f * PI;

					prev_actual_pos_f = actual_pos_f;
					prev_vel_turns = current_vel_turns;

					float iq_inertia = sysid.J_phys * current_accel_rad;
					float iq_pid = sysid.ctrl.update(error, target_vel_turns, current_vel_turns, enc.dt_decimated, sysid.max_test_torque);
					float iq_ff = ((target_rpm > 0) ? sysid.dynamic_friction : -sysid.dynamic_friction) + (sysid.B_phys * 2.0f * PI * target_vel_turns);
					prev_iq_cmd = iq_cmd;
					iq_cmd = clip<float, float>(iq_pid + iq_ff, -sysid.max_test_torque, sysid.max_test_torque);

					// Symmetric causal alignment: half-step delayed iq_cmd matches centered acceleration sample
					float iq_mag_raw = 0.5f * (iq_cmd + prev_iq_cmd) - iq_inertia;
					iq_mag_filt += iq_ema_alpha * (iq_mag_raw - iq_mag_filt);

					if (HAL_GetTick() - tmc.calibStartTime > COGGING_WARMUP_MS) {
						if (fabsf(error) > max_err_seen) max_err_seen = fabsf(error);
						if (fabsf(iq_cmd) > max_iq_cmd_used) max_iq_cmd_used = fabsf(iq_cmd);
					}
					tmc.applySafeTorque(iq_cmd);
				}
				dec_cnt++;

				if (integrated_distance < 1.0f && (HAL_GetTick() - tmc.calibStartTime > COGGING_WARMUP_MS)) {
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
					float id = (float)tmc.getActualFlux();
#endif
					float s1, c1;
					arm_sin_cos_f32(actual_pos_f * 360.0f, &s1, &c1);

					float cur_s = s1, cur_c = c1;
					for (int k = 1; k < COGGING_CALIB_DFT_HARMONICS; k++) {
						cur_cos_acc[k] += (iq_mag_filt * cur_c);
						cur_sin_acc[k] += (iq_mag_filt * cur_s);
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
						buf.id_acc_cos[k] += (id * cur_c);
						buf.id_acc_sin[k] += (id * cur_s);
#endif
						float next_c = cur_c * c1 - cur_s * s1;
						float next_s = cur_c * s1 + cur_s * c1;
						cur_c = next_c;
						cur_s = next_s;
					}
					if (p == 1) buf.fwd_samples++; else buf.bwd_samples++;
					buf.total_samples++;
					integrated_distance += fabsf(step);
				}

				refreshWatchdog();
				waitNextTick(next_tick);
			}
			tmc.stopCalibTimers();
		}
		tmc.applySafeTorque(0);
		if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

		broadcastLog(0, "Tier 0 %s done -> Dist:%.2f tr | MaxErr:%.2f deg | MaxIq:%.0f",
			(p == 1) ? "FWD" : "BWD", integrated_distance, (max_err_seen * 360.0f), max_iq_cmd_used);
	}
	return true;
}

// ============================================================================
// Step 4/4: Harmonics Extraction & Electrical/Mechanical Diagnostics
// ============================================================================

bool TMC4671CoggingCalibrator::extractHarmonics(const CoggingDftBuffers& buf, int& main_h, uint16_t& max_order, const char** err) {
	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }
	if (buf.total_samples == 0) { *err = "Abort: No DFT samples collected"; return false; }

	broadcastLog(0, "STEP 4/4: Post-Processing & Harmonics Extraction...");

	struct TempHarmonic { float32_t mag; uint16_t k; float32_t phase; };
	TempHarmonic* candidates = (TempHarmonic*)pvPortMalloc(COGGING_CALIB_DFT_HARMONICS * sizeof(TempHarmonic));
	if (!candidates) {
		*err = "Abort: Memory fail (candidates)";
		return false;
	}

	float norm = 2.0f / (float)buf.total_samples;
	if (buf.dual_dir_separated && buf.fwd_samples > 0 && buf.bwd_samples > 0) {
		float norm_f = 2.0f / (float)buf.fwd_samples;
		float norm_b = 2.0f / (float)buf.bwd_samples;
		for (int k = 1; k < COGGING_CALIB_DFT_HARMONICS; k++) {
			float re_f = buf.iq_fwd_cos[k] * norm_f;
			float im_f = buf.iq_fwd_sin[k] * norm_f;
			float mag_f = sqrtf(re_f * re_f + im_f * im_f);
			float re_b = buf.iq_acc_cos[k] * norm_b;
			float im_b = buf.iq_acc_sin[k] * norm_b;
			float mag_b = sqrtf(re_b * re_b + im_b * im_b);

			candidates[k].mag = 0.5f * (mag_f + mag_b);
			float u_re = ((mag_f > 1e-3f) ? (re_f / mag_f) : 0.0f) + ((mag_b > 1e-3f) ? (re_b / mag_b) : 0.0f);
			float u_im = ((mag_f > 1e-3f) ? (im_f / mag_f) : 0.0f) + ((mag_b > 1e-3f) ? (im_b / mag_b) : 0.0f);
			candidates[k].phase = atan2f(u_re, u_im);
			candidates[k].k = k;
		}
	} else {
		for (int k = 1; k < COGGING_CALIB_DFT_HARMONICS; k++) {
			float re = buf.iq_acc_cos[k] * norm;
			float im = buf.iq_acc_sin[k] * norm;
			candidates[k].mag = sqrtf(re * re + im * im);
			candidates[k].phase = atan2f(re, im);
			candidates[k].k = k;
		}
	}

	if (candidates[1].mag > 5000.0f) {
		broadcastLog(0, "Warning: High motor eccentricity detected (H1=%.0f)", candidates[1].mag);
	}

	int k_min = clip<int, int>((int)tmc.conf.motconf.pole_pairs, 4, 11);
	broadcastLog(0, "Selecting top %d harmonics (order >= %d, poles=%u)...", COGGING_HARMONICS_COUNT, k_min, tmc.conf.motconf.pole_pairs);
	memset(tmc.cogging_harmonics, 0, sizeof(tmc.cogging_harmonics));

	for (int n = 0; n < COGGING_HARMONICS_COUNT; n++) {
		float max_m = 0.0f;
		int best_idx = -1;
		for (int k = k_min; k < COGGING_CALIB_DFT_HARMONICS; k++) {
			bool taken = false;
			for (int j = 0; j < n; j++) {
				if (tmc.cogging_harmonics[j].order == candidates[k].k) { taken = true; break; }
			}
			if (!taken && candidates[k].mag > max_m) {
				max_m = candidates[k].mag;
				best_idx = k;
			}
		}
		if (best_idx != -1) {
			tmc.cogging_harmonics[n].order = candidates[best_idx].k;
			tmc.cogging_harmonics[n].amplitude = candidates[best_idx].mag;
			tmc.cogging_harmonics[n].phase = candidates[best_idx].phase;
			tmc.cogging_harmonics[n].speed_delta = 0;
		}
	}

#ifdef COGGING_CALIB_ENABLE_ID_DIAG
	float h3_id = sqrtf(powf(buf.id_acc_cos[3] * norm, 2) + powf(buf.id_acc_sin[3] * norm, 2));
	float h6_id = sqrtf(powf(buf.id_acc_cos[6] * norm, 2) + powf(buf.id_acc_sin[6] * norm, 2));
	if (h3_id > 500.0f || h6_id > 500.0f) {
		broadcastLog(0, "Warning: Potential electrical phase imbalance");
	}
#endif
	vPortFree(candidates);

	main_h = -1;
	float max_amp = 0.0f;
	max_order = 1;
	for (int n = 0; n < COGGING_HARMONICS_COUNT; n++) {
		if (tmc.cogging_harmonics[n].amplitude > max_amp) {
			max_amp = tmc.cogging_harmonics[n].amplitude;
			main_h = n;
		}
		if (tmc.cogging_harmonics[n].amplitude > 0.0f && tmc.cogging_harmonics[n].order > max_order) {
			max_order = tmc.cogging_harmonics[n].order;
		}
	}

	if (main_h != -1) {
		broadcastLog(0, "Tier 0 Top harmonic: H%u (Amp=%.1f, Phase=%.2f rad, MaxOrder=%u)",
			tmc.cogging_harmonics[main_h].order, tmc.cogging_harmonics[main_h].amplitude, tmc.cogging_harmonics[main_h].phase, max_order);
	}
	return true;
}

// ============================================================================
// Step 4.2/4: Multi-Speed Ripple Profiling (Tiers 1, 2, 3: 12, 36, 90 RPM)
// ============================================================================

bool TMC4671CoggingCalibrator::profileMultiSpeedRipple(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, int main_h, uint16_t max_order, const char** err) {
	if (main_h == -1 || !tmc.hasPower()) return true;
	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

	float tier3_rpm_cap = std::min<float>(90.0f, 24000.0f / (float)std::max<uint16_t>(1, max_order));
	tier3_rpm_cap = std::max<float>(45.0f, tier3_rpm_cap);
	const float tier_rpms[3] = {12.0f, 36.0f, tier3_rpm_cap};
	broadcastLog(0, "STEP 4.2/4: Multi-Speed Ripple Profiling (Tiers: %.0f, %.0f, %.0f RPM)...",
		tier_rpms[0], tier_rpms[1], tier_rpms[2]);

	float reg_num[COGGING_HARMONICS_COUNT] = {0.0f};
	float reg_den = 0.0f;
	float tau_lead_sum = 0.0f;
	int tau_lead_cnt = 0;
	float tier_dt_sec = (float)enc.base_period_us / 1000000.0f;
	float tier_ema_alpha = clip<float, float>((2.0f * PI * 180.0f * enc.dt_decimated) / (1.0f + 2.0f * PI * 180.0f * enc.dt_decimated), 0.15f, 0.9f);

	for (int t = 0; t < 3 && !isEstopActive() && tmc.hasPower(); t++) {
		float cur_tier_rpm = tier_rpms[t];
		float cur_tier_vel = cur_tier_rpm / 60.0f;
		float tier_amps_dir[2][COGGING_HARMONICS_COUNT];
		float tier_phase_main[2] = {0.0f, 0.0f};
		bool dir_ok[2] = {false, false};
		memset(tier_amps_dir, 0, sizeof(tier_amps_dir));

		const int8_t t_dirs[2] = {1, -1};
		for (int d = 0; d < 2 && !isEstopActive() && tmc.hasPower(); d++) {
			float signed_vel = (t_dirs[d] == 1) ? cur_tier_vel : -cur_tier_vel;
			float target_pos_f = tmc.getFilteredPosition();
			float prev_pos_f = target_pos_f;
			float prev_vel = signed_vel;
			float integrated_dist = 0.0f;
			uint32_t t_samples = 0;
			float acc_c[COGGING_HARMONICS_COUNT] = {0.0f};
			float acc_s[COGGING_HARMONICS_COUNT] = {0.0f};

			sysid.ctrl.reset(signed_vel);
			uint32_t t_start = HAL_GetTick();
			uint32_t t_warmup = (t == 0) ? 700 : 400;
			uint32_t t_timeout = (uint32_t)(120000.0f / cur_tier_rpm) + t_warmup + 1500;
			uint32_t next_tick = micros();
			uint32_t dec_cnt = 0;
			float iq_cmd = 0.0f;
			float prev_iq_cmd = 0.0f;
			float iq_mag_filt = 0.0f;

			tmc.startCalibTimers(TIM_TMC_ARR);
			while (integrated_dist < 1.0f && (HAL_GetTick() - t_start) < t_timeout && !isEstopActive() && tmc.hasPower()) {
				next_tick += enc.base_period_us;
				float step = signed_vel * tier_dt_sec;
				target_pos_f += step;
				if (target_pos_f >= 1.0f) target_pos_f -= 1.0f;
				if (target_pos_f < 0.0f) target_pos_f += 1.0f;

				float actual_pos_f = tmc.getFilteredPosition();
				if (dec_cnt % enc.decimation_ratio == 0) {
					float pos_err = tmc.getWrappedError(target_pos_f, actual_pos_f);
					float dpos = tmc.getWrappedError(prev_pos_f, actual_pos_f);
					float cur_vel = dpos / enc.dt_decimated;
					float cur_accel_rad = ((cur_vel - prev_vel) / enc.dt_decimated) * 2.0f * PI;
					prev_pos_f = actual_pos_f;
					prev_vel = cur_vel;

					float iq_inertia = sysid.J_phys * cur_accel_rad;
					float iq_pid = sysid.ctrl.update(pos_err, signed_vel, cur_vel, enc.dt_decimated, sysid.max_test_torque);
					float iq_ff = ((signed_vel > 0.0f) ? sysid.dynamic_friction : -sysid.dynamic_friction) + (sysid.B_phys * 2.0f * PI * signed_vel);
					prev_iq_cmd = iq_cmd;
					iq_cmd = clip<float, float>(iq_pid + iq_ff, -sysid.max_test_torque, sysid.max_test_torque);

					float iq_mag_raw = 0.5f * (iq_cmd + prev_iq_cmd) - iq_inertia;
					iq_mag_filt += tier_ema_alpha * (iq_mag_raw - iq_mag_filt);
					tmc.applySafeTorque(iq_cmd);
				}
				dec_cnt++;

				if ((HAL_GetTick() - t_start) > t_warmup) {
					float pos_deg = actual_pos_f * 360.0f;
					for (uint8_t i = 0; i < COGGING_HARMONICS_COUNT; i++) {
						if (tmc.cogging_harmonics[i].amplitude > 0.0f) {
							float s_h, c_h;
							arm_sin_cos_f32(pos_deg * (float)tmc.cogging_harmonics[i].order, &s_h, &c_h);
							acc_c[i] += iq_mag_filt * c_h;
							acc_s[i] += iq_mag_filt * s_h;
						}
					}
					t_samples++;
					integrated_dist += fabsf(step);
				}
				refreshWatchdog();
				waitNextTick(next_tick);
			}
			tmc.stopCalibTimers();
			tmc.applySafeTorque(0);
			if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

			if (t_samples > 10) {
				float t_norm = 2.0f / (float)t_samples;
				for (uint8_t i = 0; i < COGGING_HARMONICS_COUNT; i++) {
					if (tmc.cogging_harmonics[i].amplitude > 0.0f) {
						float re_i = acc_c[i] * t_norm;
						float im_i = acc_s[i] * t_norm;
						tier_amps_dir[d][i] = sqrtf(re_i * re_i + im_i * im_i);
						if (i == (uint8_t)main_h) {
							tier_phase_main[d] = atan2f(re_i, im_i);
						}
					}
				}
				dir_ok[d] = true;
			}
			if (!safeDelay(150)) { *err = "Abort: Emergency stop engaged"; return false; }
		}

		if (dir_ok[0] && dir_ok[1]) {
			float s_val = cur_tier_vel / (cur_tier_vel + TMC4671::COGGING_RIPPLE_CORNER_VEL);
			reg_den += s_val * s_val;
			for (uint8_t i = 0; i < COGGING_HARMONICS_COUNT; i++) {
				if (tmc.cogging_harmonics[i].amplitude > 0.0f) {
					float amp_tier = 0.5f * (tier_amps_dir[0][i] + tier_amps_dir[1][i]);
					reg_num[i] += s_val * (amp_tier - tmc.cogging_harmonics[i].amplitude);
				}
			}
			if (t >= 1) {
				float dphi = tier_phase_main[1] - tier_phase_main[0];
				while (dphi > PI) dphi -= 2.0f * PI;
				while (dphi < -PI) dphi += 2.0f * PI;
				float half_dphi = fabsf(0.5f * dphi);
				float w_k_rad = 2.0f * PI * (float)tmc.cogging_harmonics[main_h].order * cur_tier_vel;
				if (w_k_rad > 1.0f && half_dphi < 1.2f) {
					tau_lead_sum += (half_dphi / w_k_rad);
					tau_lead_cnt++;
				}
			}
			float main_amp_tier = 0.5f * (tier_amps_dir[0][main_h] + tier_amps_dir[1][main_h]);
			broadcastLog(0, "Tier %d (%.0f RPM, S=%.2f) -> H%u Amp: %.1f (base %.1f)",
				t + 1, cur_tier_rpm, s_val, tmc.cogging_harmonics[main_h].order, main_amp_tier, tmc.cogging_harmonics[main_h].amplitude);
		}
	}
	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

	if (reg_den > 1e-4f) {
		for (uint8_t i = 0; i < COGGING_HARMONICS_COUNT; i++) {
			float base_a = tmc.cogging_harmonics[i].amplitude;
			if (base_a > 0.0f) {
				float delta_max = clip<float, float>(reg_num[i] / reg_den, -base_a * 0.8f, base_a * 2.0f);
				tmc.cogging_harmonics[i].speed_delta = (int16_t)clip<float, float>(delta_max, -15000.0f, 15000.0f);
			}
		}
	}
	if (tau_lead_cnt > 0) {
		tmc.cogging_phase_lead_time = clip<float, float>(0.0005f + (tau_lead_sum / (float)tau_lead_cnt), 0.0002f, 0.0030f);
	}
	broadcastLog(0, "Ripple fit done -> H%u speed_delta:%d | PhaseLead: %.2f ms",
		tmc.cogging_harmonics[main_h].order, tmc.cogging_harmonics[main_h].speed_delta, tmc.cogging_phase_lead_time * 1000.0f);
	return true;
}

// ============================================================================
// Step 4.5/4: Fine-Tuning Cogging Scale (Secant + Gradient Descent)
// ============================================================================

bool TMC4671CoggingCalibrator::calibrateScale(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, int main_h, const char** err) {
	if (main_h == -1 || !tmc.hasPower()) return true;
	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

	float actual_pos_f = tmc.getAbsolutePosition();
	float target_rpm = (actual_pos_f > 0.0f) ? -enc.calib_rpm : enc.calib_rpm;
	float scale_target_vel = target_rpm / 60.0f;
	float dt_sec = (float)enc.base_period_us / 1000000.0f;
	float scale_ema_alpha = clip<float, float>((2.0f * PI * 150.0f * enc.dt_decimated) / (1.0f + 2.0f * PI * 150.0f * enc.dt_decimated), 0.1f, 0.9f);

	broadcastLog(0, "STEP 4.5/4: Fine-tuning Cogging Scale (Main harmonic: H%u, Amp: %.1f)",
		tmc.cogging_harmonics[main_h].order, tmc.cogging_harmonics[main_h].amplitude);

	float currentTestScale = 0.0f;
	float scaleZero = 0.0f, errorAtScaleZero = 0.0f;
	float scaleOne = 1.0f, errorAtScaleOne = 0.0f;
	float gradientLearningRate = 0.0f;
	float previousResidualError = 0.0f;
	float optimalScaleFound = 0.0f;
	float lowestAbsoluteError = 9999999.0f;
	sysid.ctrl.reset(scale_target_vel);

	for (int iter = 0; iter < 8 && !isEstopActive() && tmc.hasPower(); iter++) {
		if (iter == 0) {
			currentTestScale = 0.0f;
			scaleZero = currentTestScale;
		} else if (iter == 1) {
			currentTestScale = 1.0f;
			scaleOne = currentTestScale;
		} else if (iter == 2) {
			gradientLearningRate = (fabsf(errorAtScaleOne - errorAtScaleZero) > 1.0f)
				? fabsf((scaleOne - scaleZero) / (errorAtScaleOne - errorAtScaleZero))
				: 0.0f;
			currentTestScale = scaleOne + errorAtScaleOne * gradientLearningRate;
		} else {
			currentTestScale = currentTestScale + previousResidualError * (gradientLearningRate * 0.5f);
		}

		currentTestScale = clip<float, float>(currentTestScale, -2.0f, 2.0f);
		tmc.cogging_scale = currentTestScale;

		float target_pos_move = actual_pos_f;
		float segment_dist = 0.0f;
		float periods = ceilf(0.3f * (float)tmc.cogging_harmonics[main_h].order);
		float segment_target = periods / (float)tmc.cogging_harmonics[main_h].order;

		float acc_re = 0.0f, acc_im = 0.0f;
		uint32_t samples = 0;
		uint32_t next_tick = micros();
		uint32_t iter_start_tick = HAL_GetTick();
		uint32_t iter_timeout = (uint32_t)((segment_target * 60000.0f) / enc.calib_rpm) + 3000;
		uint32_t dec_cnt = 0;
		float iq_cmd = 0.0f, prev_iq_cmd = 0.0f, iq_mag_filt = 0.0f;
		float scale_prev_pos = actual_pos_f;
		float scale_prev_vel = scale_target_vel;

		tmc.startCalibTimers(TIM_TMC_ARR);
		while (segment_dist < segment_target && (HAL_GetTick() - iter_start_tick) < iter_timeout && !isEstopActive() && tmc.hasPower()) {
			next_tick += enc.base_period_us;
			float step = scale_target_vel * dt_sec;
			target_pos_move += step;
			actual_pos_f = tmc.getAbsolutePosition();

			if (dec_cnt % enc.decimation_ratio == 0) {
				float error = target_pos_move - actual_pos_f;
				float raw_vel = (actual_pos_f - scale_prev_pos) / enc.dt_decimated;
				float accel_rad = ((raw_vel - scale_prev_vel) / enc.dt_decimated) * 2.0f * PI;
				scale_prev_pos = actual_pos_f;
				scale_prev_vel = raw_vel;

				float iq_inertia = sysid.J_phys * accel_rad;
				float iq_pid = sysid.ctrl.update(error, scale_target_vel, raw_vel, enc.dt_decimated, sysid.max_test_torque);
				float iq_ff = ((target_rpm > 0) ? sysid.dynamic_friction : -sysid.dynamic_friction) + (sysid.B_phys * 2.0f * PI * scale_target_vel);
				prev_iq_cmd = iq_cmd;
				iq_cmd = clip<float, float>(iq_pid + iq_ff, -sysid.max_test_torque, sysid.max_test_torque);

				float iq_mag_raw = 0.5f * (iq_cmd + prev_iq_cmd) - iq_inertia;
				iq_mag_filt += scale_ema_alpha * (iq_mag_raw - iq_mag_filt);

				float angle_rad = actual_pos_f * 2.0f * PI;
				float compensation = 0.0f;
				for (uint8_t i = 0; i < COGGING_HARMONICS_COUNT; i++) {
					if (tmc.cogging_harmonics[i].amplitude > 0.0f) {
						compensation += tmc.cogging_harmonics[i].amplitude * arm_sin_f32(angle_rad * tmc.cogging_harmonics[i].order + tmc.cogging_harmonics[i].phase);
					}
				}
				tmc.applySafeTorque(clip<float, float>(iq_cmd + (tmc.cogging_scale * compensation), -sysid.max_test_torque, sysid.max_test_torque));
			}
			dec_cnt++;

			if (HAL_GetTick() - iter_start_tick > 200) {
				float s, c;
				arm_sin_cos_f32(actual_pos_f * 360.0f * tmc.cogging_harmonics[main_h].order, &s, &c);
				acc_re += iq_mag_filt * c;
				acc_im += iq_mag_filt * s;
				samples++;
				segment_dist += fabsf(step);
			}
			refreshWatchdog();
			waitNextTick(next_tick);
		}
		tmc.stopCalibTimers();
		tmc.applySafeTorque(0);
		if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }
		if (samples == 0) break;

		float re = acc_re / (float)samples;
		float im = acc_im / (float)samples;
		float dot = re * sinf(tmc.cogging_harmonics[main_h].phase) + im * cosf(tmc.cogging_harmonics[main_h].phase);
		previousResidualError = dot;

		if (fabsf(dot) < lowestAbsoluteError) {
			lowestAbsoluteError = fabsf(dot);
			optimalScaleFound = currentTestScale;
		}
		if (iter == 0) errorAtScaleZero = dot;
		if (iter == 1) errorAtScaleOne = dot;

		broadcastLog(0, "Scale Iter %d/8: Scale %.4f -> Residual %.2f Iq", iter + 1, currentTestScale, dot);
	}

	tmc.cogging_scale = optimalScaleFound;
	tmc.cogging_enabled = true;
	broadcastLog(0, "Optimal cogging scale identified: %.4f (Err: %.2f)", optimalScaleFound, lowestAbsoluteError);
	return true;
}

// ============================================================================
// Step 5: Return to Center (Unwinding Multi-Turn)
// ============================================================================

bool TMC4671CoggingCalibrator::returnToCenter(const CoggingEncoderProfile& enc, CoggingSysIdResult& sysid, const char** err) {
	if (!tmc.hasPower()) return true;
	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }

	float actual_pos_f = tmc.getAbsolutePosition();
	broadcastLog(0, "Return to center: Start pos = %.3f turns", actual_pos_f);

	float target_pos_f = actual_pos_f;
	float target_rpm = (actual_pos_f > 0.0f) ? -enc.calib_rpm : enc.calib_rpm;
	float ret_vel_turns = target_rpm / 60.0f;
	float ret_dt_sec = (float)enc.base_period_us / 1000000.0f;
	float ret_prev_pos = actual_pos_f;
	uint32_t ret_dec_cnt = 0;
	sysid.ctrl.reset(ret_vel_turns);

	float distance_turns = fabsf(actual_pos_f);
	uint32_t dynamic_timeout_ms = (uint32_t)((distance_turns * 60000.0f) / enc.calib_rpm) + 5000;
	uint32_t return_start = HAL_GetTick();
	uint32_t next_tick = micros();

	tmc.startCalibTimers(TIM_TMC_ARR);
	while (fabsf(actual_pos_f) > 0.005f && !isEstopActive() && tmc.hasPower()) {
		next_tick += enc.base_period_us;
		target_pos_f += ret_vel_turns * ret_dt_sec;
		if (target_rpm < 0 && target_pos_f < 0.0f) target_pos_f = 0.0f;
		if (target_rpm > 0 && target_pos_f > 0.0f) target_pos_f = 0.0f;

		actual_pos_f = tmc.getAbsolutePosition();
		if (ret_dec_cnt % enc.decimation_ratio == 0) {
			float error = target_pos_f - actual_pos_f;
			float raw_vel = (actual_pos_f - ret_prev_pos) / enc.dt_decimated;
			ret_prev_pos = actual_pos_f;
			float cur_ref_vel = (target_pos_f == 0.0f) ? 0.0f : ret_vel_turns;
			float iq_pid = sysid.ctrl.update(error, cur_ref_vel, raw_vel, enc.dt_decimated, sysid.max_test_torque);
			float iq_ff = ((cur_ref_vel > 0.0f) ? sysid.dynamic_friction : ((cur_ref_vel < 0.0f) ? -sysid.dynamic_friction : 0.0f))
				+ (sysid.B_phys * 2.0f * PI * cur_ref_vel);
			tmc.applySafeTorque(clip<float, float>(iq_pid + iq_ff, -sysid.max_test_torque, sysid.max_test_torque));
		}
		ret_dec_cnt++;

		refreshWatchdog();
		if (HAL_GetTick() - return_start > dynamic_timeout_ms) {
			broadcastLog(0, "Return to center timeout");
			break;
		}
		waitNextTick(next_tick);
	}
	tmc.stopCalibTimers();
	tmc.applySafeTorque(0);

	if (isEstopActive()) { *err = "Abort: Emergency stop engaged"; return false; }
	broadcastLog(0, "Centering done. Re-aligning encoder...");
	return true;
}

// ============================================================================
// Main Orchestrator: TMC4671CoggingCalibrator::run()
// ============================================================================

void TMC4671CoggingCalibrator::run() {
	const char* errorMessage = nullptr;
	char errBuf[128] = {0};
	CoggingDftBuffers dftBuf;

	tmc.prevCalibMode = tmc.getMotionMode();
	TMC4671PIDConf prevPids = tmc.curPids;
	TMC4671PIDConf calibPids = tmc.curPids;

	broadcastLog(0, "Starting Cogging Calibration: Continuous DFT...");

	if (tmc.getCpr() == 0) {
		errorMessage = "Abort: CPR is 0";
		goto cleanup;
	}

	tmc.allowStateChange = false;
	tmc.clearCoggingTable();

	calibPids.velocityP = 1000;
	calibPids.velocityI = 100;
	tmc.setPids(calibPids);

	tmc.curFilters.flux.params.enable = false;
	tmc.setBiquadFlux(tmc.curFilters.flux);

	// 1. Allocate DFT accumulators on FreeRTOS heap
	dftBuf.iq_acc_cos = (float*)pvPortMalloc(COGGING_CALIB_DFT_HARMONICS * sizeof(float));
	dftBuf.iq_acc_sin = (float*)pvPortMalloc(COGGING_CALIB_DFT_HARMONICS * sizeof(float));
	dftBuf.iq_fwd_cos = (float*)pvPortMalloc(COGGING_CALIB_DFT_HARMONICS * sizeof(float));
	dftBuf.iq_fwd_sin = (float*)pvPortMalloc(COGGING_CALIB_DFT_HARMONICS * sizeof(float));
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
	dftBuf.id_acc_cos = (float*)pvPortMalloc(COGGING_CALIB_DFT_HARMONICS * sizeof(float));
	dftBuf.id_acc_sin = (float*)pvPortMalloc(COGGING_CALIB_DFT_HARMONICS * sizeof(float));
#endif

	if (!dftBuf.iq_acc_cos || !dftBuf.iq_acc_sin || !dftBuf.iq_fwd_cos || !dftBuf.iq_fwd_sin
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
		|| !dftBuf.id_acc_cos || !dftBuf.id_acc_sin
#endif
	) {
		errorMessage = "Abort: Memory fail (Heap)";
		goto cleanup;
	}

	memset(dftBuf.iq_acc_cos, 0, COGGING_CALIB_DFT_HARMONICS * sizeof(float));
	memset(dftBuf.iq_acc_sin, 0, COGGING_CALIB_DFT_HARMONICS * sizeof(float));
	memset(dftBuf.iq_fwd_cos, 0, COGGING_CALIB_DFT_HARMONICS * sizeof(float));
	memset(dftBuf.iq_fwd_sin, 0, COGGING_CALIB_DFT_HARMONICS * sizeof(float));
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
	memset(dftBuf.id_acc_cos, 0, COGGING_CALIB_DFT_HARMONICS * sizeof(float));
	memset(dftBuf.id_acc_sin, 0, COGGING_CALIB_DFT_HARMONICS * sizeof(float));
#endif

	// 2. Run modular calibration pipeline
	{
		int main_h = -1;
		uint16_t max_order = 1;

		if (!tmc.hasPower()) {
			runSimulationDft(dftBuf);
			if (!extractHarmonics(dftBuf, main_h, max_order, &errorMessage)) goto cleanup;
			tmc.saveCoggingTable();
		} else {
			if (isEstopActive()) {
				errorMessage = "Abort: Emergency stop engaged";
				goto cleanup;
			}
			tmc.startMotor();
			tmc.setMotionMode(MotionMode::torque, true);

			CoggingEncoderProfile enc;
			CoggingSysIdResult sysid;

			if (!identifyEncoder(enc, &errorMessage))                                 goto cleanup;
			if (!runSysId(enc, sysid, &errorMessage))                                 goto cleanup;
			if (!validateTracking(enc, sysid, errBuf, sizeof(errBuf), &errorMessage)) goto cleanup;
			if (!acquireTier0Dft(enc, sysid, dftBuf, &errorMessage))                  goto cleanup;
			if (!extractHarmonics(dftBuf, main_h, max_order, &errorMessage))          goto cleanup;
			if (!profileMultiSpeedRipple(enc, sysid, main_h, max_order, &errorMessage)) goto cleanup;

			tmc.saveCoggingTable();
			broadcastLog(0, "Cogging & ripple table saved to flash");

			if (!calibrateScale(enc, sysid, main_h, &errorMessage))                   goto cleanup;
			if (!returnToCenter(enc, sysid, &errorMessage))                           goto cleanup;
		}
	}

cleanup:
	tmc.stopCalibTimers();
	tmc.setFluxTorque(0, 0);
	if (isEstopActive()) {
		tmc.enablePin.reset();
		tmc.setMotionMode(MotionMode::stop, true);
		tmc.setPwm(TMC_PwmMode::off);
		if (!errorMessage) {
			errorMessage = "Abort: Emergency stop engaged";
		}
	}

	if (errorMessage) {
		broadcastLog(1, errorMessage);
	} else {
		broadcastLog(1, "Cogging detection finished");
	}

	if (!isEstopActive()) {
		tmc.setTargetVelocity(0);
	}

	if (dftBuf.iq_acc_cos) vPortFree(dftBuf.iq_acc_cos);
	if (dftBuf.iq_acc_sin) vPortFree(dftBuf.iq_acc_sin);
	if (dftBuf.iq_fwd_cos) vPortFree(dftBuf.iq_fwd_cos);
	if (dftBuf.iq_fwd_sin) vPortFree(dftBuf.iq_fwd_sin);
#ifdef COGGING_CALIB_ENABLE_ID_DIAG
	if (dftBuf.id_acc_cos) vPortFree(dftBuf.id_acc_cos);
	if (dftBuf.id_acc_sin) vPortFree(dftBuf.id_acc_sin);
#endif

	tmc.setPids(prevPids);
	tmc.curFilters.flux.params.enable = true;
	tmc.setBiquadFlux(tmc.curFilters.flux);
	if (!isEstopActive()) {
		tmc.setMotionMode(tmc.prevCalibMode, true);
	}

	tmc.allowStateChange = true;
	if (isEstopActive()) {
		tmc.motorEnabledRequested = false;
		tmc.changeState(TMC_ControlState::Shutdown, false);
		tmc.changeState(TMC_ControlState::Shutdown, true);
	} else if (errorMessage) {
		tmc.changeState(tmc.hasPower() ? TMC_ControlState::Running : TMC_ControlState::waitPower);
	} else {
		tmc.changeState(TMC_ControlState::EncoderInit);
	}
}

#endif // TMC4671DRIVER && COGGING_TABLE_FLASH_START_ADDRESS
