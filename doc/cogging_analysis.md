# Anti-Cogging, Dynamic Speed-Ripple & Motor Diagnostic Analysis (Continuous Multi-Speed DFT-128 Method)

This document describes the physical principles, mathematical design, and firmware implementation of the harmonic-based anti-cogging, dynamic speed-dependent torque ripple compensation, and motor diagnostic system for the OpenFFBoard (TMC4671).

---

## 1. Motor Typology & Electromechanical Physics

Direct-drive motors used with OpenFFBoard span two orders of magnitude in rotor inertia ($J$) and over an order of magnitude in pole-pair count ($P$). Understanding these physical regimes is essential to tuning the calibration speed loop and harmonic compensation across all hardware:

| Physical Characteristic | **Heavy Industrial Servos**<br>*(e.g., MiGe 130ST-M10010 / M15015)* | **Small Direct-Drive / Gimbal / Outrunners**<br>*(e.g., K52G, Hoverboard, 60ST/80ST)* | **Hybrid Stepper Motors**<br>*(e.g., NEMA 23 / NEMA 34)* | **Low-Cogging / Skewed Servos**<br>*(e.g., Kollmorgen AKM, Tamagawa)* |
| :--- | :--- | :--- | :--- | :--- |
| **Rotor Inertia ($J$)** | **Very High** ($J_{tmc} \approx 500\text{–}1500$) | **Very Low** ($J_{tmc} \approx 15\text{–}120$) | **Low–Medium** ($J_{tmc} \approx 40\text{–}250$), near-zero viscous damping | **Medium–High** ($J_{tmc} \approx 200\text{–}800$) |
| **Pole Pairs ($P$) & Slots ($N_s$)** | $P = 4\text{–}5$, $N_s = 12\text{–}15$ | $P = 7\text{–}15$, $N_s = 12\text{–}36$ | **$P = 50$** (100 poles) | $P = 4\text{–}6$, skewed stator/rotor |
| **Dominant Harmonic Orders ($k$)** | Low spatial orders ($k = 12, 15, 24, 30, 36$) | Mid/High orders ($k = 14, 21, 24, 36, 42, 72$) | Very high orders ($k = 50, 100$) | Low detent cogging; load/speed ripple at $k = 6P, 12P$ |
| **Cogging Acceleration Ratio ($\frac{T_{cog}}{J}$)** | **Low** — Rotor mass naturally low-pass filters cogging pulses | **Critical ($10\times\text{–}50\times$ higher)** — Each slot tooth induces violent instantaneous acceleration ($\alpha = T_{cog}/J$) | **Critical & Resonant** — Magnetic tooth stiffness ($k \cdot A_k$) creates a mass-spring resonance | **Low** — Dominated by Back-EMF & eddy-current ripple at speed |
| **Coulomb Friction / Inertia Ratio ($\frac{T_C}{J}$)** | Low to moderate | **Very High** — Bearing/seal stiction dominates the light rotor | **High** + strong magnetic hysteresis | Moderate, linear viscous drag |

### Why Small Motors Require Active Velocity Damping ($K_v$) and Dry/Viscous Friction Separation
At the nominal slow calibration speed (**3.0 RPM** = 20 s/rev), a light rotor stores negligible kinetic energy. When approaching a magnetic tooth of stiffness $K_{cog} = k \cdot A_k$, the rotor decelerates rapidly toward $0\text{ RPM}$ and locks into **magnetic stick-slip**.
* A pure position PI controller ($K_d = 0$) driving a double-integrator plant $P_\theta(s) = \frac{1}{J s^2 + B s}$ yields a 3rd-order closed-loop characteristic polynomial $J s^3 + B s^2 + K_p s + K_i = 0$. By the **Routh–Hurwitz criterion**, stability requires $B \cdot K_p > J \cdot K_i$. On low-damping small motors ($B \to 0$), this condition is violated and the rotor oscillates violently.
* Furthermore, if Coulomb dry friction $T_C$ is not separated from viscous damping $B$, the over-estimated $B$ term subtracted in $K_p = 2\zeta\omega_n J - B$ drives $K_p$ negative (clamping it to the floor) on small motors where $T_C / J$ is large.
* **Solution**: Separate $T_C$ and $B_{phys}$ across two speeds over the **exact same angular sector**, compute $J_{phys}$ from the **net torque** $(T_{pulse} - T_C)$, and control the motor with a **Cascade Position + Active Velocity Damping ($K_p, K_v, K_i$) controller with conditional Anti-Windup**.

---

## 2. Process Overview

The system is encapsulated in the dedicated `TMC4671CoggingCalibrator` class (`TMC4671_cogging.h` / `TMC4671_cogging.cpp`) and uses a **Continuous Multi-Speed Discrete Fourier Transform (DFT)** integration. It eliminates lookup tables and large time-domain buffers, making it memory-safe for all supported microcontrollers (F407 and F411). The design follows a **"Thorough Calibration / Ultra-Fast Real-Time Execution"** philosophy: all heavy system identification, multi-speed regression, and phase-lead extraction occur offline during calibration so that `TMC4671::turn()` executes deterministically with minimal CPU cost.

1.  **Step 1 — True Physical Encoder Identification & Dynamic Decimation**:
    *   Audits the **true physical encoder resolution** (`abnconf.cpr` for internal ABN, ADC-interpolated `clip(aencconf.cpr, 1, 16) * 4096` for SinCos/UVW, or `drvEncoder->getCpr()` for external encoders) rather than the TMC4671's internal 16-bit `0x10000` (65536) scaling register.
    *   Calculates `enc_decimation_ratio` mathematically to guarantee **$\ge 4$ physical encoder ticks per PID/acceleration evaluation** at `calib_rpm` (default **3.0 RPM** = 20 s/rev):
        $$\text{enc\_decimation\_ratio} = \text{clip}\left(\left\lceil \frac{4 \cdot f_{timer}}{\text{CPR}_{phys} \cdot (\text{calib\_rpm}/60)} \right\rceil,\ 1,\ 16\right)$$
    *   For low-resolution encoders (`< 16384 CPR`), `calib_rpm` is automatically raised (6–10 RPM) so the decimated control loop stays $\ge 250\text{ Hz}$.
    *   Applies a smooth discrete phase-margin stiffness penalty: $\text{resolution\_penalty} = \text{clip}(1 / \sqrt{\text{enc\_decimation\_ratio}},\ 0.25,\ 1.0)$.

2.  **Step 2 — Physical System Identification (SysID) & Cascade Controller Tuning**:
    *   **Step 2.1 — Fine Static Friction (Breakout)**: Ramps torque from `clip(0.05 * bangInitPower, 50, 250)` in fine `+50 count` increments (below 500) to avoid jolting small low-inertia motors.
    *   **Step 2.2 — Two-Speed Friction Separation ($T_C$ vs $B_{phys}$) on the Identical 90° Sector**: Measures mean steady-state torque at **15 RPM** and **45 RPM** while traversing the **exact same 0.25-turn angular sector** $[\theta_{start} + 0.05,\ \theta_{start} + 0.30]$. Because both passes integrate over the exact same physical angle, local cogging and asymmetric steering wheel gravitational imbalance cancel out identically in the difference:
        $$B_{phys} = \text{clip}\left(\frac{\bar{T}_{45} - \bar{T}_{15}}{\omega_{45} - \omega_{15}},\ 0,\ 1000\right), \qquad T_C = \bar{T}_{15} - B_{phys} \cdot \omega_{15}$$
    *   **Step 2.3 — Dual-Direction Mechanical Inertia ($J_{phys}$)**: Applies a strong torque pulse ($2.2 \times T_{breakout}$, capped at $0.75 \cdot T_{max}$) in both `+1` and `-1` directions (max 120 ms, early-exit at 0.20 turns to prevent Back-EMF saturation on light rotors). Averaging `+1` and `-1` cancels the local cogging tooth slope, and subtracting $T_C$ isolates the net accelerating torque:
        $$J_{phys} = \frac{(T_{pulse} - T_C) \cdot \Delta t^2}{2 \cdot \Delta \theta_{rad}}$$
    *   **Step 2.4 — Cascade Pole Placement ($K_p, K_v, K_i$) & Conditional Anti-Windup**:
        Using homogeneous turn units ($J_{turns} = 2\pi J_{phys}$, $B_{turns} = 2\pi B_{phys}$) and inertia-scaled bandwidth $f_{bw} = \text{clip}(16.5 - 0.0047 \cdot J,\ 6.0,\ 15.0)\text{ Hz}$ ($\omega_n = 2\pi f_{bw}$):
        *   $K_p = \text{clip}(1.6 \cdot \omega_n^2 \cdot J_{turns} \cdot \text{pen},\ 250,\ 250000)$
        *   $K_v = \text{clip}((2.2 \cdot \omega_n \cdot J_{turns} - B_{turns}) \cdot \text{pen},\ 0,\ 0.35 \cdot J_{turns} / \Delta t_{dec})$ *(active velocity damping, capped at the discrete Nyquist stability limit with EMA-filtered velocity at $3 f_{bw}$)*
        *   $K_i = \text{clip}(0.35 \cdot \omega_n^3 \cdot J_{turns} \cdot \text{pen}^2,\ 10,\ 200000)$ *(continuous integral gain multiplied by $\Delta t_{dec}$, invariant to timer rate or decimation, with conditional anti-windup clamping)*
    *   **Manual PID Override**: If either `coggingSpeedP` or `coggingSpeedI` is non-zero, SysID is bypassed and manual gains are injected.
    *   **Step 2.5 — Stability Validation Rotation**: A 2.5s tracking check at `calib_rpm` clocked at the unified `TIM_TMC_ARR` frequency. Aborts cleanly if tracking error exceeds `5.0°`.

3.  **Step 3 — Tier 0 Static Cogging DFT (`calib_rpm`, Default 3.0 RPM = 20s/rev)**:
    *   Clocked strictly at `TIM_TMC_ARR` (4kHz / 250µs or 5kHz / 200µs). Integrates exactly 1.0 revolution Forward and 1.0 revolution Backward after `COGGING_WARMUP_MS`.
    *   **24-Bit FPU Trigonometric Seed (`sinf` / `cosf`)** *(fix `f7709946`)*: Replaces the 512-entry CMSIS LUT (`arm_sin_cos_f32`) with exact IEEE-754 single-precision `sinf(angle_rad)` / `cosf(angle_rad)` for the fundamental harmonic seed before angle-addition recurrence, preserving full 21/22-bit encoder precision (BiSS-C, MT6835, >16-bit ABN) up to H127 without accumulated LUT interpolation drift.
    *   **Consistent Forward Velocity Sign** *(fix `ecfe72e9`)*: Computes wrapped displacement as `getWrappedError(actual_pos_f, prev_actual_pos_f)` (`actual - prev`), ensuring stabilizing active velocity damping ($K_v$) and accurate physical sign for causal inertia subtraction in both Tier 0 and multi-speed passes.
    *   **Separate FWD/BWD Accumulators**: Instead of summing FWD and BWD into a single accumulator (which attenuates amplitude by $\cos(\delta_k)$ when residual phase lag $\delta_k$ exists), FWD (`iq_fwd_cos/sin`) and BWD (`iq_acc_cos/sin`) are accumulated separately. The true unattenuated magnitude is $A_{k,0} = \frac{1}{2}(|\vec{V}_{k,FWD}| + |\vec{V}_{k,BWD}|)$ and the exact static magnetic phase is the unit-vector bisector between FWD and BWD.
    *   **Causal Real-Time Inertia Subtraction (Why FWD+BWD Addition Cannot Cancel Inertia)**:
        *   By the chain rule, angular acceleration as a function of spatial angle $\theta$ is $\alpha(\theta) = \frac{d\omega}{dt} = \omega \frac{d\omega}{d\theta} = \frac{1}{2}\frac{d(\omega^2)}{d\theta}$.
        *   Because $\alpha(\theta)$ depends on $\omega^2$, it is an **even function of velocity**: it has the **exact same spatial sign in both Forward and Backward passes** (unlike friction, which is odd and cancels in FWD+BWD). Thus, adding FWD and BWD *doubles* inertial artifacts instead of canceling them.
        *   Subtracting $J_{phys} \cdot \alpha$ in real time reconstructs 100% of the pure magnetic torque $-T_{cog}(\theta) = I_{q,cmd} - J_{phys}\alpha - T_{friction}$, even for high-order harmonics beyond the speed controller's bandwidth (and up to 95% of the ripple signal during high-speed passes!).
        *   **Symmetric Causal Alignment**: To avoid asymmetric phase shift between `iq_cmd` and `iq_inertia`, `iq_cmd` is half-step delayed (`0.5f * (iq_cmd + prev_iq_cmd)`) to align with the centered acceleration derivative, and a **single shared EMA filter** (~150 Hz) is applied to the combined signal `iq_mag_raw = 0.5*(iq_cmd + prev_iq_cmd) - iq_inertia`.
    *   **Adaptive Harmonic Selection**: Selects the top 20 peaks for orders $k \ge k_{min}$ where $k_{min} = \text{clip}(\text{pole\_pairs},\ 4,\ 11)$, ignoring H1/H2 wheel gravitational imbalance while supporting low-pole-count motors.

4.  **Step 4.2 — Multi-Speed Ripple Profiling (Tiers 1, 2, 3: 12 RPM, 36 RPM, up to 90 RPM)**:
    *   **Physics of Speed-Dependent Torque Ripple (0..300 RPM)**:
        *   Non-sinusoidal Back-EMF harmonics ($5, 7, 11, 13$ electrical $\rightarrow$ $6P, 12P$ mechanical) and stator tooth eddy currents generate torque ripple that grows with speed $\omega$ as $k \cdot \omega$ exceeds the TMC4671 internal current PI bandwidth, then **plateaus toward an inductive asymptote** at high speed ($100\text{–}300\text{ RPM}$) where winding reactance $j k \omega L$ limits current ripple ($\Delta I_q \sim \frac{\Delta E_k}{k\omega L} \to \text{const}$).
        *   Measuring only two close speeds (e.g., 3 and 18 RPM) and linearly extrapolating to 300 RPM would amplify noise by $\sim 20\times$ and diverge past the inductive plateau.
    *   **Geometrically Spaced 4-Tier Profiling (Zero Net Unwinding)**:
        *   Performs 1.0 rev FWD + 1.0 rev BWD (returning to 0.0 net turns at each tier, taking only ~17 s total) at **12 RPM**, **36 RPM**, and **$\min(90\text{ RPM},\ 24000 / k_{max})$** (Nyquist-safe cap for high-pole steppers).
        *   Computes a targeted 20-harmonic DFT on `iq_mag_filt` at each tier and performs a **least-squares regression** onto the bounded RL/PI asymptotic model $S(\omega) = \frac{|\omega|}{|\omega| + \omega_c}$ ($\omega_c = 0.6\text{ turns/s} = 36\text{ RPM}$):
            $$\Delta A_{i,\max} = \frac{\sum_{m=1}^{3} S_m \cdot (A_i(\omega_m) - A_{i,0})}{\sum_{m=1}^{3} S_m^2}$$
        *   Stores $\Delta A_{i,\max}$ in `int16_t speed_delta` inside the existing 2-byte tail padding of `struct Harmonic` (keeping `sizeof(Harmonic) == 12` bytes and `COGGING_TABLE_SIZE == 240` bytes unchanged!).
        *   Directly extracts the real motor+driver **phase-lead latency** $\tau_{lead}$ (`cogging_phase_lead_time`, clamped to `[0.2 ms, 3.0 ms]`) from the FWD/BWD phase difference on the main harmonic at Tiers 2 & 3 *(kept in RAM; defaults to `0.8 ms` on reboot, while `speed_delta` is persisted in Flash)*.

5.  **Step 4.5 — Scale Calibration (Hybrid Secant + Gradient Descent at `TIM_TMC_ARR`)**:
    *   Runs at the same unified `TIM_TMC_ARR` frequency as Tier 0, evaluates harmonic synthesis and residual DFT projection on **`getFilteredPosition()`** *(fix `f7709946`)* (matching the exact phase origin of Tier 0 DFT and `TMC4671::turn()`), and computes the residual dot product on the causal inertia-subtracted torque `iq_mag_filt` (preventing scale bias from rotor inertia), with exact integer-period segments to prevent spectral leakage and an absolute-best safeguard.

6.  **Step 5 — Post-Acquisition Homing & Direct Safe Exit**:
    *   Uses the full tuned cascade controller at `TIM_TMC_ARR` to unwind back to `0.0` (with dynamic timeout, *fix `e3ae51b4`*).
    *   Transitions **directly** to `Running`, `Shutdown`, or `waitPower` (via `enterWaitPower()`) without re-running `EncoderInit` (unless `!encoderAligned`) and without injecting a spurious `setTargetVelocity(0)` step, preserving the calibrated magnetic phase origin *(fix `f7709946`)*.

---

## 3. Technical Specifications

### Memory Efficiency (Zero-Buffer Goal)
*   **DFT Accumulators**: Separate FWD/BWD 128-float pairs (`iq_acc_cos/sin`, `iq_fwd_cos/sin`, plus optional `id_acc_cos/sin`).
*   **Peak Heap RAM**: ~3 KB on the FreeRTOS heap during calibration, freed unconditionally in `cleanup:`.
*   **TMC Thread Stack Usage**: **1,016 bytes** worst-case static call chain (`Run()` → `handleStateCoggingCalibration()` → `TMC4671CoggingCalibrator::run()` [352 B] → `profileMultiSpeedRipple()` [640 B], verified via `-fstack-usage` in `TMC4671.su` and `TMC4671_cogging.su`), safely within `TMC_THREAD_MEM` (4096 bytes).
*   **Permanent Flash / RAM**: Exactly **240 bytes** (`20 * 12 bytes`, `static_assert(sizeof(Harmonic) == 12)`). Legacy flash tables are automatically sanitized in `restoreFlash()` against NaN/Inf and out-of-range padding bytes.
*   **Compatibility**: Verified across **F407VG**, **F407VG_DISCO**, and **F411RE**.

### Internal Tuning Constants
*   `COGGING_CALIB_LUT_RESOLUTION` (2880): Legacy protocol compatibility.
*   `COGGING_CALIB_TIME_PER_REV_S` (20): Tier 0 revolution duration (**20s = 3.0 RPM**).
*   `COGGING_CALIB_DFT_HARMONICS` (128): Analyzed harmonic spectrum.
*   `COGGING_RIPPLE_CORNER_VEL` (0.6f): Corner speed ($0.6\text{ turns/s} = 36\text{ RPM}$) for the bounded RL/PI asymptotic ripple model.
*   `VAL_TOTAL_DURATION_MS` (2500ms): Duration of the stability validation check.
*   `COGGING_WARMUP_MS` (1500ms): Stabilization window ignored before Tier 0 DFT integration.

---

## 4. Calibration Phase Summary (Steps & Timings)

| Phase | Sub-Step | Duration | Control Strategy | Goal |
| :--- | :--- | :--- | :--- | :--- |
| **1. Encoder ID** | True CPR Audit | Instant | Mathematical Decimation ($\ge 4\text{ ticks/step}$) | Determine physical CPR (`abnconf.cpr`, SinCos interp, or ext) and `enc_decimation_ratio` |
| **2. SysID** | 2.1 Breakout | Variable | Fine torque ramp (`+50` / `+100`) | Measure static friction threshold |
| | 2.2 Two-Speed Friction | ~3.5s | 15 RPM & 45 RPM on identical 90° sector | Separate Coulomb $T_C$ from Viscous $B_{phys}$ without cogging/imbalance bias |
| | 2.3 Dual-Dir Inertia | ~1.0s | $+1/-1$ pulses ($120\text{ ms}$ or $0.20\text{ tr}$) | Measure true $J_{phys}$ from net torque $(T_{pulse} - T_C)$ |
| | 2.4 Cascade Tuning | Instant | Pole Placement ($K_p, K_v, K_i$) + Anti-Windup | Guarantee stability on both heavy servos and light small motors |
| | 2.5 Validation | 2500ms | Cascade Control @ `TIM_TMC_ARR` | Verify tracking error $\le 5.0^\circ$ |
| **3. Tier 0 DFT** | Static Cogging | ~42s | FWD + BWD (1.0 rev each) @ 3.0 RPM | Capture 128-harmonic static cogging with separated FWD/BWD & causal inertia |
| **4.2 Ripple Profile** | Tiers 1, 2, 3 | ~17s | FWD + BWD (1.0 rev each) @ 12, 36, 90 RPM | Least-squares fit of asymptotic `speed_delta` and `cogging_phase_lead_time` |
| **4.5 Scaling** | Secant + Gradient | ~15s | Cascade Control @ `TIM_TMC_ARR` (8 iters) | Refine `cogging_scale` on causal residual `iq_mag_filt` |
| **5. Return** | Centering & Exit | Variable | Cascade Ramp to `0.0` $\rightarrow$ Direct `Running`/`Shutdown` | Unwind to center and resume without re-running `EncoderInit` |

---

## 5. Diagnostics & Engineering Points

### Point 1: Electrical Diagnostic (`COGGING_CALIB_ENABLE_ID_DIAG`)
*   Analyzes the **Id axis** (Flux). Significant energy in **H3** or **H6** signals phase imbalance or partial winding short.

### Point 2: Mechanical Diagnostic (Eccentricity)
*   Analyzes **H1** magnitude on the Iq axis. High H1 indicates rotor eccentricity, bent shaft, or encoder misalignment.

### Point 3: Adaptive Harmonic Anti-Cogging
*   Scans all 128 harmonics and selects the **Top 20 peaks** with order $k \ge \text{clip}(\text{pole\_pairs}, 4, 11)$, rejecting low-order gravitational wheel imbalance while capturing fundamental cogging even on low-pole-pair motors.

---

## 6. Real-Time Compensation Formula (`TMC4671::turn()`)

Implemented in `turn()` for ultra-fast deterministic execution (zero extra SPI reads, zero dynamic allocation, phase-lead and speed factor hoisted outside the 20-harmonic loop):

1. **Instantaneous Speed Estimation**: Filtered wrapped difference of `pos_f` over `dt_us = micros() - last_turn_us` (guarded by `50 < dt_us < 50000`).
2. **Bounded RL/PI Asymptotic Speed Factor** *(computed once outside loop)*:
   $$S(\omega) = \frac{|\omega|}{|\omega| + \omega_c} \in [0, 1]$$
3. **Predictive Phase-Lead** *(factored once outside loop — automatically applies $\Delta\phi_k = 2\pi k \omega \tau_{lead}$ to every harmonic $k$ at zero extra trigonometric cost)*:
   $$\theta_{pred} = \theta + \omega \cdot \tau_{lead}$$
4. **Fourier Synthesis with ZOH Anti-Aliasing Roll-off ($200\text{–}350\text{ Hz}$)**:
   $$T_{comp}(\theta, \omega) = \sum_{n=1}^{20} G_{ZOH}(Order_n, \omega) \cdot \text{clip}\left(A_n + \Delta A_{n,\max} \cdot S(\omega),\ 0,\; 3 A_n\right) \cdot \sin\left(2\pi \cdot Order_n \cdot \theta_{pred} + \Phi_n\right)$$
   $$T_{final} = T_{game\_dir} + \text{clip}\left( Scale \cdot T_{comp}(\theta, \omega),\ -T_{lim},\ +T_{lim} \right)$$
   * **`invertForce` Decoupling** *(fix `f7709946`)*: Axis-level `conf.invertForce` (used with `combineEncoder`) is applied *only* to the incoming game FFB torque (`T_game_dir = invertForce ? -power : power`), never to internal anticogging compensation or `applySafeTorque()`. Total motor torque is inverted only when commutating through a reversed external encoder (`encoderReversed && enctype == ext`).
   * **Scale Clamping** *(fix `ecfe72e9`)*: `coggingScale` commands are clamped to `[-1.5, 1.5]` to prevent unstable over-compensation.

---

## 7. Timer, Pacing and Parameter Reference

*   **`externalEncoderTimer`**: `&TIM_TMC` (typically `htim6`, ARR = `TIM_TMC_ARR`). Drives `extEncUpdater` and paces calibration via `calibTicksCount` / `calibTicksTarget` when an external encoder is active. `waitNextTick()` waits on `TIM_TMC` (or `TIM_CALIBRATION`) with a **bounded 50 ms timeout** (`WaitForNotification(50)`) to prevent watchdog deadlocks if a timer stops *(fix `f7709946`)*.
*   **`calibTimer`**: `&TIM_CALIBRATION` (typically `htim9`, shared with `TIM_USER`). Dynamically configured to `TIM_TMC_ARR` µs when no external encoder is active.
*   **`enc_decimation_ratio`**: Computed dynamically from true physical CPR and `calib_rpm` to guarantee $\ge 4$ physical ticks per control/acceleration evaluation.
*   **`resolution_penalty`**: `clip(1.0f / sqrtf(enc_decimation_ratio), 0.25f, 1.0f)` applied to cascade gains for discrete phase margin.
*   **`coggingSpeedP` & `coggingSpeedI`**: Optional manual overrides (default `0.0f` enables full automatic SysID).
*   **`cogging_phase_lead_time`**: Calibrated predictive phase-lead latency $\tau_{lead}$ in seconds (default `0.0008f`, bounded `[0.0002f, 0.0030f]`).

---

## 8. Encoder Precision, `zeroenc` Invariance and Architecture

### 16-Bit Internal Wrap-Around (`0x10000` vs `0xffff`) & Full-Resolution Absolute Anchoring
1. **Exact 65536 Counts/Rev in `TMC4671::getCpr()`** *(fix `1570bd31`)*: The TMC4671 internal 16-bit `PhiM` / position register (`0x6B`) wraps at exactly **$2^{16} = 65536$ (`0x10000`)** counts per mechanical revolution. Returning `0x10000` (instead of `0xffff`) eliminates a 1-count-per-turn multi-revolution phase drift ($0.55^\circ$/turn on H100) while preserving exact scaling in `Encoder::getPos_f()` and `Axis.cpp`.
2. **True Physical CPR Separation** *(fix `1570bd31`)*: While `getCpr()` returns the position-register scale (`0x10000` internally or `drvEncoder->getCpr()` externally), calibration queries the physical encoder CPR (`abnconf.cpr`, interpolated `aencconf.cpr`, or external CPR) so internal ABN encoders (e.g., 4,000–10,000 CPR) are properly decimated instead of being misclassified as 65,536 CPR High-Res encoders.
3. **Full-Resolution `zeroenc`-Invariant `getFilteredPosition()`** *(fix `f7709946`)*: Anchors cogging phase to the physical mechanical angle so user wheel centering (`zeroenc` / `setPos()`) never shifts cogging phase, while performing integer modulo *before* float conversion to prevent mantissa truncation across unlimited turns:
   * **External Encoders** (`BiSS-C`, `MT6825`, `MT6835`, etc.): `drvEncoder->getPosAbs() % drvEncoder->getCpr()` (preserves full 21/22-bit resolution).
   * **Internal ABN**: Raw 32-bit `ABN_DECODER_COUNT` (`0x27`) `% abnconf.cpr` shifted by `abnconf.phiMoffset / 65536.0f` (preserves >16-bit ABN resolution and `zeroenc` invariance).
   * **Internal SinCos / UVW**: Hardware mechanical angle `PHI_M_AENC` (`0x46 & 0xffff`).
   * **Internal Hall**: Hardware mechanical angle `PHI_M_HALL` (`0x3A & 0xffff`).
4. **External Encoder Boot Preservation** *(fix `cb541de9`)*: `allowExternalEncoder` defaults to `true` when `TIM_TMC` is compiled in, preventing boot-time fallback to `EncoderType_TMC::NONE` before `Axis::setEncType()` attaches the external encoder instance.

---

## 9. Safety, E-Stop, and State-Machine Hardening

1. **Calibration Pre-Conditions & Command Guards**:
   * `calibrateCogging` rejects powered calibration if `emergency`/`estopTriggered`/`brake_failure` is active, `state == HardError`, another calibration is busy (`isCalibrationInProgress() && !allowStateChange`), `motor_type == NONE`, or `!encoderAligned || !adcCalibrated || enctype == NONE` *(fix `f7709946`)*.
   * Mutating configuration and calibration commands (`cpr`, `mtype`, `encsrc`, `poles`, `tmcHwType`, `fullCalibration`, `encalign`, `findIndex`, `pidautotune`) are rejected while a calibration is actively running or `HardError` is latched *(fix `ecfe72e9`)*.
2. **Immediate Multi-Layer E-Stop & Brake Resistor Protection**:
   * **0 ms Hardware E-Stop Engagement** *(fix `cb52dc22`)*: `FFBHIDMain::exti()` engages E-stop immediately on `E_STOP_Pin` activation (the 1000 ms debounce applies *only* to release) and cuts `DRV_ENABLE` inside the ISR without blocking SPI calls (`inIsr()` guards). `FLAG_Pin` `statusCheck()` SPI reads are also deferred from the EXTI ISR to the TMC thread *(fix `bca4590d`)*.
   * **External `ENABLE_IN` Polling** *(fix `f7709946`, `a9cc36a7`)*: During cogging calibration, `isEstopActive()` polls `E_STOP_Pin`, `brake_failure`, and TMC4671 `ENABLE_IN` (`0x76` bit 15) every 50 ms via `checkHardwareEstop()` using a per-instance timestamp (`last_en_check_tick`).
   * **Brake Resistor Fault (`brake_failure`)** *(fix `b9a5e0d6`)*: Exported from `voltagesense.h` and checked in `canRunMotor()`, `motorReady()`, `startMotor()`, `Run()`, and `isEstopActive()` to abort any calibration within milliseconds and permanently latch `HardError`.
   * **Strict `HardError` Absorption** *(fix `a9cc36a7`)*: Once entered (via `enterHardError()`), `changeState()` and `Run()` refuse all transitions until MCU reset, with `DRV_ENABLE` low, `PWM_FOC` off, and torque zeroed.
3. **Torque Clamping & Deterministic State Transitions**:
   * `applySafeTorque()` and all TMC4671 torque/flux setters clamp outputs via `clampTorqueFlux()` (`curLimits.pid_torque_flux` or `30000`) and force `0` when E-stop is active *(fix `f8a70a97`)*.
   * Centralized helpers (`disablePowerStage()`, `enablePowerStageForCalibration()`, `enterShutdown()`, `enterWaitPower()`, `enterHardError()`) enforce clean power-stage cutoffs and deterministic recovery on power loss, E-stop, or blank flash (`MotorType::NONE`) *(fix `a9cc36a7`)*.
   * `Axis::updateDriveTorque()` zeros accumulated endstop/slew-rate torque during any calibration or unready state, and `Axis::prepareForUpdate()` re-arms the soft force fade-in (`0 -> 1.0`) upon returning to `Running` *(fix `cb541de9`)*.
   * Cross-thread/ISR state and E-stop flags are qualified `volatile` *(fix `cb52dc22`, `a9cc36a7`)*; `~TMC4671()` stops `TIM_TMC`/`TIM_CALIBRATION`, resets `extEncUpdater`, and disables the power stage safely *(fix `f8a70a97`)*.
   * Encoder, AENC, and ADC scaling routines guard against division-by-zero, null pointers, and `int16` overflow *(fix `c9db85dc`)*.

---

## 10. Bug-Fix Traceability (`feat/anticogging`)

| Commit | Scope | Fixed Bugs / Hardening |
| :--- | :--- | :--- |
| `ecfe72e9` | `TMC4671`, `TMC4671_cogging` | Inverted velocity sign in Tier 0 & multi-speed DFT passes, `isSetUp()` null deref, uninitialized `encofs` on blank flash, missing command guards during calibration/faults, unclamped `coggingScale` |
| `f7709946` | `TMC4671`, `TMC4671_cogging` | `zeroenc` phase shift & >16-bit precision loss in `getFilteredPosition()`, 512-LUT `arm_sin_cos_f32` DFT drift (replaced with 24-bit FPU `sinf`/`cosf`), `invertForce` flipping anticogging sign, missing `TIM_TMC` pacing / 50 ms timeout / `ENABLE_IN` polling, missing `calibrateCogging` preconditions, unwanted `EncoderInit` on exit |
| `cb541de9` | `Axis`, `TMC4671` | Boot-time external encoder fallback (`allowExternalEncoder`), premature `encoderAligned=true`, uninitialized Axis power limit on blank flash, endstop torque accumulation & missing fade-in re-arm during calibration, `checkEncoder` missing `bangInitEnc` on reversal |
| `f8a70a97` | `TMC4671`, `TMC4671_cogging` | E-stop bypass & missing torque clamping (`clampTorqueFlux`), missing `calibrateAenc` 15 s timeout, `AutoPID` leaving `fluxP=20000` on failure, `checkEncoder` wrapping & `enc_retry` bugs, bitwise mask typos, unsafe `~TMC4671()` destructor |
| `c9db85dc` | `TMC4671` | Division-by-zero and `int16`/32-bit overflow in encoder, AENC, and ADC offset scaling (`zeroAbnUsingPhiM`, `getPhiEfromExternalEncoder`, `encToPos`, `posToEnc`, `calibrateAenc`, `calibrateAdcOffset`) |
| `bca4590d` | `TMC4671` | Blocking SPI `statusCheck()` executed inside `FLAG_Pin` EXTI ISR (deferred to TMC thread) |
| `b9a5e0d6` | `voltagesense`, `TMC4671`, `TMC4671_cogging` | `brake_failure` flag ignored by TMC4671 during motor drive and cogging calibration |
| `a9cc36a7` | `TMC4671` | Power stage left active in fault/stop states, non-absorbing `HardError`, `Shutdown` deadlock via `Suspend()`, `MotorType::NONE` PWM enable, non-volatile state flags |
| `cb52dc22` | `FFBHIDMain`, `TMC4671` | 1000 ms E-stop engagement delay in `exti()`, `errorCallback()` clearing E-stop on critical errors, blocking SPI in ISR (`inIsr()`), non-volatile E-stop flags |
| `1570bd31` | `TMC4671` | `getCpr()` returning `0xffff` instead of `0x10000` (1-count/turn phase drift), physical CPR misclassification on internal encoders, dynamic PID decimation |
| `e3ae51b4` | `TMC4671` | Missing dynamic timeout on post-acquisition return-to-home ramp |
