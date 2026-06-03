# Anti-Cogging and Motor Diagnostic Analysis (Continuous DFT-128 Method)

This document describes the implementation of the harmonic-based anti-cogging compensation and motor diagnostic system for the OpenFFBoard (TMC4671).

## 1. Process Overview

The system uses a **Continuous Discrete Fourier Transform (DFT)** integration. It eliminates lookup tables and fixed-size buffers, making it memory-safe for all supported microcontrollers (F407 and F411).

1.  **Software PID & System Identification**: Before acquisition, the system performs a deterministic identification of the motor's physical parameters to calculate ideal PID gains (using CMSIS-DSP). It avoids heuristic trial-and-error by measuring the actual hardware:
    *   **Encoder Identification**: Audits the encoder resolution (`enc_cpr`) to mathematically decimate the PID execution rate (e.g., down to 1kHz or 500Hz) for low-resolution encoders. This allows enough physical ticks to accumulate, guaranteeing a stable derivative signal for inertia feedforward without amplifying pure quantization noise. The system logs the determined encoder performance class (Low/Medium/High-Res) and chosen constants.
    *   **Static Friction (Breakout)**: Identifies the minimum torque required to overcome stiction.
    *   **Mechanical Inertia ($J$)**: Measures angular acceleration ($\alpha$) resulting from a constant torque pulse ($\tau$). $J = \tau / \alpha$.
    *   **Viscous Friction ($B$)**: Measures the steady-state torque required to maintain a constant moderate speed ($\omega$). $B = \tau / \omega$.
    *   **Deterministic Gain Calculation (Pole Placement / IMC)**: Instead of Ziegler-Nichols, the system uses exact physics-based formulas to guarantee a **critically damped response** ($\zeta = 1.0$) at a target control bandwidth ($f_{bw}$).
        *   **Bandwidth ($f_{bw}$)**: Linearly degraded based on inertia ($J$). Low inertia gives high bandwidth (up to 15Hz), high inertia gives lower bandwidth (down to 6Hz) to prevent $K_p$ saturation. Formula: $f_{bw} = 16.5 - 0.0047 \cdot J$.
        *   **Integral Scale ($ki_{scale}$)**: Inversely proportional to $J$ ($0.3 / J$) and scaled for the loop frequency in kHz ($/ freq\_khz$) to prevent integral windup on heavy motors. The frequency `freq_khz` is dynamically calculated from `TIM_TMC_ARR` (e.g. 4.0 for 4kHz, 5.0 for 5kHz).
        *   $K_p = 2 \cdot \zeta \cdot (2\pi \cdot f_{bw}) \cdot J - B$ (Clamped up to 250,000 for high-frequency stiffness).
        *   $K_i = (2\pi \cdot f_{bw})^2 \cdot J \cdot ki_{scale}$
        *   $K_d = 0$ (Forced to zero for maximum stability at low speed).
    *   **Manual PID Override**: If either `coggingSpeedP` or `coggingSpeedI` is set to a non-zero value, the physical identification phase (Breakout, Inertia $J$, Viscous Friction $B$, and IMC Calculation) is entirely bypassed. The software PID directly uses these manual overrides for $K_p$ and $K_i$. If both are `0.0f` (default), the automatic identification is executed.
    *   **Validation Rotation**: A single 2.5s test rotation at **calib_rpm** is performed to verify tracking stability. If the error exceeds 5.0°, calibration aborts.

2.  **Torque Response Capture (Deterministic Dual-Pass Acquisition)**: The motor rotates at a constant speed defined by **calib_rpm** (default: 7.5 RPM) in both directions.
    *   **High-Fidelity Integration**: Acquisition is strictly clocked at the configured `TIM_TMC` frequency (e.g., 4kHz / 250µs or 5kHz / 200µs). The system waits for a stabilization period (`COGGING_WARMUP_MS`) and then integrates the torque over exactly 360° of displacement (`integrated_distance < 1.0f`).
    *   **Real-Time Inertia Feedforward**: At the active loop frequency, the PID must output torque to overcome even microscopic accelerations. The system calculates real-time angular acceleration ($\alpha$) and dynamically subtracts the inertial energy ($iq\_inertia = J \cdot \alpha$) from the measured torque. The DFT only processes the "pure" magnetic cogging signature, eliminating phase lag and distortion.
    *   **Torque Inversion & Safety**: The system supports `conf.invertForce` during torque application via `applySafeTorque`, preventing positive feedback loops in various hardware configurations.
    *   **Continuous Mathematical Wrapping**: The system uses `floorf`-based modulo arithmetic for position wrapping ($pos - \lfloor pos \rfloor$) on both target and actual positions, ensuring robust tracking across multiple revolutions.
    *   **Complex Rotation Optimization**: Uses recursive complex multiplication ($e^{i(k+1)\theta} = e^{ik\theta} \cdot e^{i\theta}$) to calculate 128 harmonics with only one trigonometric call per sample.

3.  **Scale Calibration (Hybrid Secant + Gradient Descent)**: Fine-tuning of the compensation amplitude (`cogging_scale`).
    *   **Phase 1 - Secant Approximation**: Two initial measurements are taken at 0% and 100% compensation. Using the resulting residual errors, a secant line is calculated to mathematically jump to the theoretical optimal scale. The inverse slope of this secant line perfectly represents the system's proportional gain regardless of motor torque or units.
    *   **Phase 2 - Gradient Descent Refinement**: For the remaining iterations (up to 8 total), the system uses the gain extracted from the Secant phase (halved for stability) as a proportional controller. This dynamically refines the scale to compensate for any physical non-linearities, driving the error to the absolute measurement noise floor.
    *   **Absolute Best Safeguard**: A safeguard mechanism records the scale that yielded the lowest absolute residual error across all 8 iterations and strictly applies this maximum-precision value at the end.
    *   **Spectral Leakage Prevention**: Each test iteration calculates an exact integer number of periods for the main harmonic (minimum 0.3 turns). This prevents fractional-period integration, which would destroy the dot product phase correlation.

4.  **Post-Acquisition Homing & Re-alignment**: After successful analysis, the system ensures the motor returns to a known hardware state.
    *   **Multi-turn Unwinding**: Using the full tuned PID gains at the aligned frequency (4kHz or 5kHz), the motor performs a controlled absolute position ramp towards `0.0`. This "unwinds" any revolutions accumulated during the acquisition or retry phases.
    *   **Encoder Re-Zeroing**: Upon reaching the origin, the driver is automatically switched to the `EncoderInit` state. This triggers a hardware re-alignment and resets the encoder position registers.

## 2. Technical Specifications

### Memory Efficiency (The Zero-Buffer Goal)
*   **Accumulators**: Uses 128 pairs of `float` (32-bit) values to leverage the hardware FPU.
*   **Peak RAM**: Approximately **2 KB** on the FreeRTOS heap during calibration.
*   **Permanent RAM**: Only **240 bytes** (20 saved harmonics).
*   **Compatibility**: Fully portable between F407 and F411.

### Internal Tuning Constants
*   `MAX_TOLERANCE_DEG` (3.0°): Absolute maximum error limit allowed during DFT.
*   `VAL_TOTAL_DURATION_MS` (2500ms): Duration of the PID validation check.
*   `COGGING_WARMUP_MS` (1500ms): Stabilization window ignored during measurement and integration.

## 3. Calibration Phase Summary (Steps & Timings)

The following table summarizes the sequence of operations, their durations, and the torque strategies used to ensure stability.

| Phase | Sub-Step | Duration | Torque / Control Strategy | Goal |
| :--- | :--- | :--- | :--- | :--- |
| **1. Encoder ID** | Profiling | - | Mathematical Decimation | Audit hardware resolution |
| **2. SysID** | Breakout | Variable | Ramp-up | Measure static friction |
| | Inertia ($J$) | 150ms | Constant Pulse | Measure rotor mass |
| | Friction ($B$) | 2000ms | Velocity P-Loop | Measure dynamic drag |
| **3. Validation** | Sanity Check | 2500ms | PID Control (Calculated Gains) | Verify tracking stability |
| **4. Acquisition** | Setup | 1000ms | Zero Torque | Settle motor at rest |
| | DFT Integration | ~12.0s (Max) | PID Control (Wait 1500ms before DFT) @ Aligned Rate (e.g. 4kHz/5kHz) | Capture exactly 360° of pure cogging (inertia subtracted) |
| **4.5. Scaling**| Hybrid Secant/Gradient | ~15.0s | PID Control @ Aligned Rate (e.g. 4kHz/5kHz) | Mathematical approximation and proportional refinement of `cogging_scale` |
| **5. Return** | Centering | Variable | PID Control (Absolute Ramp to 0.0) @ Aligned Rate (e.g. 4kHz/5kHz) | Unwind motor revolutions |
| | Final Align | - | `EncoderInit` State | Reset hardware alignment |

### Encoder Profiling Constants
The calibration speed (`calib_rpm`) and PID execution rate are precalculated automatically based on the hardware resolution. This matrix ensures that even 12-bit magnetic encoders can achieve successful calibration without violent torque oscillations:

| Encoder Class | CPR Range | PID Rate (Decimation) | Kp Penalty | Calibration Speed | Justification |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **High-Res** | > 50,000 | 4kHz/5kHz (Ratio: 1) | 1.0x | 7.5 RPM | Infinite resolution allows full-speed loop and purest gain calculation |
| **Medium-Res** | 20,000 - 50,000 | 1kHz (Ratio: 4) | 0.5x | 6.0 RPM | Lower execution rate requires slight gain dampening for Nyquist stability |
| **Low-Res** | < 20,000 | 500Hz (Ratio: 8) | 0.2x | 12.0 RPM | Heavy dampening and faster spin needed to accumulate enough ticks per loop |

### Configuration Macros
*   `COGGING_CALIB_LUT_RESOLUTION`: Standardized at 2880 points for communication protocol compatibility.
*   `COGGING_CALIB_TIME_PER_REV_S`: Time in seconds to complete one revolution (Default: 8s).
*   `COGGING_CALIB_DFT_HARMONICS`: Number of analyzed harmonics (128).
*   `COGGING_CALIB_ENABLE_ID_DIAG`: Macro to enable electrical phase analysis on the Id axis.

## 4. Design Choices & Timing Analysis

### Why 8 Seconds per Revolution?
This speed (**7.5 RPM**) is carefully selected as a "Physical Sweet Spot":
*   **Avoiding Stick-Slip**: It is fast enough to ensure the motor remains in the "viscous friction" regime, avoiding the jerky "stick-slip" motion caused by static friction.
*   **Negligible Dynamics**: It is slow enough that Back-EMF and gross phase lag are negligible. 

### Why 4kHz/5kHz (High-Frequency) Sampling & Control?
The system is synchronized directly to the target configuration's `TIM_TMC` rate (typically 4kHz / 250µs or 5kHz / 200µs) for both the PID and DFT to achieve robotic-grade force control:
*   **Dead-Time Reduction**: A 4kHz/5kHz loop dramatically reduces phase latency between position reading and torque application. This allows the PID proportional gain ($K_p$) to be pushed significantly higher without oscillating, resulting in a much "stiffer" and more accurate tracking of the 7.5 RPM target.
*   **High-Resolution Accumulation**: Running at this speed provides a dense data set for the continuous Fourier Transform, naturally filtering out high-frequency electrical noise and SPI jitter.
*   **Inertial Disaggregation**: At 4kHz/5kHz, the discrete derivative of velocity (acceleration) is highly responsive. This enables the calculation of real-time inertial torque ($J \cdot \alpha$), which is subtracted from the PID output before feeding the DFT. This prevents the algorithm from mistakenly compensating for the motor's own mass.
*   **SPI Feasibility**: A standard TMC4671 SPI read takes ~6µs. At 4kHz/5kHz, communication overhead remains negligible (~3% CPU time), safely allowing the STM32F407 to process the complex math without RTOS deadline violations.
*   **Dynamic Math Scaling**: All formulas dynamically adapt to the active period (`period_us` read from `TIM_TMC_ARR`). The integration timestep is set via $dt\_sec = period\_us / 1000000.0f$. The continuous integral gain scale $ki\_scale$ is scaled by dividing by the loop frequency in kHz ($freq\_khz = 1000.0f / TIM\_TMC\_ARR$) to prevent windup and guarantee stability regardless of the target's timer speed.

## 5. Diagnostics & Engineering Points

### Point 1: Electrical Diagnostic (`COGGING_CALIB_ENABLE_ID_DIAG`)
*   Analyzes the **Id axis** (Flux).
*   In a healthy motor, the flux remains DC. Significant energy in **H3** or **H6** indicates a phase imbalance or partial winding short.

### Point 2: Mechanical Diagnostic (Eccentricity)
*   Analyzes the **H1** magnitude on the Iq axis.
*   A high H1 magnitude signals rotor eccentricity, a bent shaft, or encoder misalignment.

### Point 3: Harmonic Anti-Cogging
*   The system scans all 128 harmonics.
*   It selects the **Top 20 peaks** with an **Order > 10**.
*   This captures the high-frequency magnetic "detent" while ignoring the low-frequency gravitational imbalance of asymmetric steering wheels.

## 6. Real-Time Compensation Formula

Implemented in the `turn()` method for zero latency:

$$T_{comp}(\theta) = \sum_{n=1}^{20} A_n \cdot \sin(Order_n \cdot \theta + \Phi_n)$$
$$T_{final} = T_{requested} + \left( Scale \cdot T_{comp}(\theta) \right)$$

## 7. Timer, Pacing and Parameter Reference

For developers modifying the driver or timing subsystem, here is a detailed reference of the variables controlling calibration pacing, external encoder synchronization, and override parameters:

### `externalEncoderTimer`
*   **Type**: `TIM_HandleTypeDef*`
*   **Utility**: Points to the hardware timer used to drive the external encoder updater thread (`TIM_TMC`, typically `htim6`). When an external encoder is active, this timer *must* continue running without being stopped or reconfigured to prevent interrupting the position feedback loop. In this scenario, the calibration logic uses this timer as a synchronous pacing clock.
*   **Expected Value**: `&TIM_TMC` (configured with `TIM_TMC_ARR` defining the tick period in microseconds) or `nullptr` if `TIM_TMC` is not defined.

### `calibTimer`
*   **Type**: `TIM_HandleTypeDef*`
*   **Utility**: Points to the hardware timer used for pacing the calibration thread (`TIM_CALIBRATION`, typically `htim9`) when *no* external encoder is used. In this case, `calibTimer` can be reconfigured dynamically to generate interrupts at the exact requested period (e.g., 200/250 microseconds for fast calibration loops, 1000 microseconds for slow calibration loops).
*   **Expected Value**: `&TIM_CALIBRATION` or `nullptr` if `TIM_CALIBRATION` is not defined.

### `calibTicksCount`
*   **Type**: `volatile uint32_t`
*   **Utility**: Active tick accumulator incremented within the `TIM_TMC` ISR. Used only when pacing the calibration via the external encoder timer (i.e. `usingExternalEncoder()` is true).
*   **Expected Value / Range**: Increments from 0 up to (calibTicksTarget - 1). Resets to 0 once `calibTicksTarget` is reached.

### `calibTicksTarget`
*   **Type**: `volatile uint32_t`
*   **Utility**: The threshold number of ticks from the `externalEncoderTimer` needed to match the requested calibration period. For instance, if `TIM_TMC_ARR` is 250 (4 kHz) and a 1000 us (1 kHz) loop step is requested, `calibTicksTarget` is set to `1000 / 250 = 4`. A value of `0` indicates that tick-based pacing is disabled (e.g., calibration is idle or using `calibTimer`).
*   **Expected Value**: Typically `1` for fast loops, `4` for slow loops (assuming a 250 us timer ARR), or `0` when inactive.

### `extEncUpdater`
*   **Type**: `std::unique_ptr<TMC_ExternalEncoderUpdateThread>`
*   **Utility**: Manages a dedicated FreeRTOS helper thread that asynchronously writes the external encoder position to the TMC4671 register `0x1C` via SPI. This thread is notified from the timer ISR to keep SPI writes out of the interrupt context.
*   **Expected Value**: Valid `std::unique_ptr` instance when an external encoder is configured, or `nullptr` otherwise.

### `enc_decimation_ratio`
*   **Type**: `uint32_t`
*   **Utility**: An execution rate divider that decouples the mathematical PID loop from the physical hardware timer. If the SPI loop runs at 4kHz and the ratio is 4, the PID is only evaluated at 1kHz. This is strictly required for encoders with low CPR to prevent severe quantization noise in the acceleration derivative.
*   **Expected Value**: `1`, `4`, or `8` depending on the initial CPR audit.

### `resolution_penalty`
*   **Type**: `float`
*   **Utility**: A multiplier applied to the critically damped proportional gain ($K_p$). Running a control loop at a lower decimated rate (e.g. 500Hz instead of 4kHz) reduces the phase margin, making the motor prone to violent high-frequency oscillations if the original stiff 4kHz gain is applied. The penalty mathematically relaxes the stiffness.
*   **Expected Value**: `1.0f` (High-Res), `0.5f` (Med-Res), or `0.2f` (Low-Res).

### `coggingSpeedP` & `coggingSpeedI`
*   **Type**: `float`
*   **Utility**: Optional manual speed loop Proportional ($K_p$) and Integral ($K_i$) gains. If either parameter is non-zero, the system identification steps (measuring $J$, $B$, static friction, and IMC calculation) are bypassed, and these gains are directly injected.
*   **Expected Value**: `0.0f` (default, to use auto-identification) or manual PID constants.

## 8. Encoder Precision and Architecture

### Float Mantissa Truncation (The 24-bit Limit)
The system requires absolute position tracking across multiple turns for robust calibration and FFB effects. However, storing absolute position directly in a 32-bit `float` introduces severe precision loss. A standard IEEE 754 32-bit float only provides 24 bits of mantissa. High-resolution encoders (e.g., 22-bit BISS-C) inherently use most of this mantissa just for a single fraction of a turn. Accumulating full turns (e.g., `1000.xxxx`) quickly pushes the fractional data out of the 24-bit window, truncating the lower bits of the encoder. This results in devastating quantization noise for the PID and severe phase-drift in the anti-cogging Fourier series over long sessions.

**The Fix**:
1. **Mathematical Protection in `Encoder::getPos_f()`**: Instead of casting the full 32-bit position integer to float, the system separates the turns from the fraction via integer arithmetic: `turns = pos / cpr; remainder = pos % cpr`. The float is only constructed at the very end (`turns + remainder / cpr`), preserving maximum fractional precision before the hardware limits apply.
2. **Infinite Precision in `getFilteredPosition()`**: For anti-cogging and harmonic calculations, full turns are mathematically irrelevant. `getFilteredPosition()` performs a strict integer modulo `pos % cpr` *before* any float conversion. This guarantees 100% of the encoder's raw resolution is maintained indefinitely, regardless of how many turns the motor has completed. This protected pure-fractional position is fed directly to the `turn()` method for the Fourier compensation phase (`arm_sin_f32`).

### Polymorphic Encoder Routing (`activeEnc`)
The firmware supports both internal (TMC4671 native pins) and external (STM32 pins) encoders. Rather than relying on redundant and prone-to-error `if/else` checks, the system unifies encoder access using a polymorphic pointer:
```cpp
Encoder* activeEnc = usingExternalEncoder() ? drvEncoder : this;
```
Because both `TMC4671` (the driver itself, `this`) and external drivers (e.g., `EncoderBissC`) inherit from the base `Encoder` class, calling `activeEnc->getPos_f()` securely routes the execution to the active hardware while automatically inheriting the mantissa protection logic. Note that the `usingExternalEncoder()` inline function intrinsically validates the `drvEncoder != nullptr` condition, ensuring robust memory safety without redundant checks.

### Low-Resolution Encoder Support (PID Decimation)
For low-resolution encoders (e.g., < 4000 CPR), the 4kHz/5kHz control loop is too fast: the encoder may not physically tick between two consecutive PID cycles, producing a discrete `delta_pos` of zero, followed by a massive spike. This aggressively amplifies noise in the derivative and acceleration terms (Inertia Feedforward).
**The Fix**: An `enc_decimation_ratio` dynamically scales the PID execution rate based on the encoder's physical CPR. High-res encoders run at the full 4kHz loop, while low-res encoders execute the PID calculation every N cycles (e.g., 500Hz). This allows enough physical time for the encoder to accumulate ticks, creating a smooth, high-fidelity `delta_pos` signal for the Inertia feedforward, while the main loop continues at maximum frequency to keep the TMC4671 SPI watchdogs satisfied.
