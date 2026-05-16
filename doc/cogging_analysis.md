# Anti-Cogging and Motor Diagnostic Analysis (Continuous DFT-128 Method)

This document describes the implementation of the harmonic-based anti-cogging compensation and motor diagnostic system for the OpenFFBoard (TMC4671).

## 1. Process Overview

The system uses a **Continuous Discrete Fourier Transform (DFT)** integration. It eliminates lookup tables and fixed-size buffers, making it memory-safe for all supported microcontrollers (F407 and F411).

1.  **Software PID & System Identification**: Before acquisition, the system performs a deterministic identification of the motor's physical parameters to calculate ideal PID gains (using CMSIS-DSP). It avoids heuristic trial-and-error by measuring the actual hardware:
    *   **Static Friction (Breakout)**: Identifies the minimum torque required to overcome stiction.
    *   **Mechanical Inertia ($J$)**: Measures angular acceleration ($\alpha$) resulting from a constant torque pulse ($\tau$). $J = \tau / \alpha$.
    *   **Viscous Friction ($B$)**: Measures the steady-state torque required to maintain a constant moderate speed ($\omega$). $B = \tau / \omega$.
    *   **Deterministic Gain Calculation (Pole Placement / IMC)**: Instead of Ziegler-Nichols, the system uses exact physics-based formulas to guarantee a **critically damped response** ($\zeta = 1.0$) at a target control bandwidth ($\omega_n$).
        *   $K_p = 2 \cdot \zeta \cdot \omega_n \cdot J - B$
        *   $K_i = \omega_n^2 \cdot J$
        *   $K_d = 0$ (Forced to zero for maximum stability at low speed).
    *   **Validation Rotation**: A single 2.5s test rotation at **TARGET_RPM** is performed to verify tracking stability. If the error exceeds 5.0°, calibration aborts.

2.  **Torque Response Capture (Deterministic Dual-Pass Acquisition)**: The motor rotates at a constant speed defined by **TARGET_RPM** (default: 8 RPM) in both directions.
    *   **1kHz Strict Integration**: Acquisition is strictly clocked at 1kHz. For an 8-second tour, exactly **8,000 samples** are integrated. This ensures perfect spatial alignment and mathematical precision for the DFT.
    *   **Torque Inversion & Safety**: The system supports `conf.invertForce` during torque application via `applySafeTorque`, preventing positive feedback loops in various hardware configurations.
    *   **Continuous Mathematical Wrapping**: The system uses `floorf`-based modulo arithmetic for position wrapping ($pos - \lfloor pos \rfloor$) on both target and actual positions, ensuring robust tracking across multiple revolutions.
    *   **Actual Current Feedback**: The system integrates the **actual currents (Iq/Id)** read from the TMC hardware registers, capturing the real physical interaction between the motor and the magnetic cogging.
    *   **Complex Rotation Optimization**: Uses recursive complex multiplication ($e^{i(k+1)\theta} = e^{ik\theta} \cdot e^{i\theta}$) to calculate 128 harmonics with only one trigonometric call per sample.
3.  **Post-Acquisition Homing & Re-alignment**: After successful analysis, the system ensures the motor returns to a known hardware state.
    *   **Multi-turn Unwinding**: Using the best-found PID gains, the motor performs a controlled ramp-down to the absolute `0.0` position. This "unwinds" any revolutions accumulated during the acquisition or retry phases.
    *   **Encoder Re-Zeroing**: Upon reaching the origin, the driver is automatically switched to the `EncoderInit` state. This triggers a hardware re-alignment (`bangInitEnc`) and resets the encoder position registers to zero.

## 2. Technical Specifications

### Memory Efficiency (The Zero-Buffer Goal)
*   **Accumulators**: Uses 128 pairs of `float` (32-bit) values to leverage the hardware FPU.
*   **Peak RAM**: Approximately **2 KB** on the FreeRTOS heap during calibration.
*   **Permanent RAM**: Only **240 bytes** (20 saved harmonics).
*   **Compatibility**: Fully portable between F407 and F411.

### Internal Tuning Constants
*   `TARGET_RPM` (7.5): The constant rotation speed for calibration.
*   `MAX_TOLERANCE_DEG` (3.0°): Absolute maximum error limit allowed during DFT.
*   `VAL_TOTAL_DURATION_MS` (2500ms): Duration of the PID validation check.
*   `COGGING_WARMUP_MS` (1500ms): Stabilization window ignored during measurement and integration.

## 3. Calibration Phase Summary (Steps & Timings)

The following table summarizes the sequence of operations, their durations, and the torque strategies used to ensure stability.

| Phase | Sub-Step | Duration | Torque / Control Strategy | Goal |
| :--- | :--- | :--- | :--- | :--- |
| **1. SysID** | Breakout | Variable | Ramp-up | Measure static friction |
| | Inertia ($J$) | 150ms | Constant Pulse | Measure rotor mass |
| | Friction ($B$) | 2000ms | Velocity P-Loop | Measure dynamic drag |
| **2. Validation** | Sanity Check | 2500ms | PID Control (Calculated Gains) | Verify tracking stability |
| **3. Acquisition** | DFT Integration | ~10.5s | PID Control (Wait 1500ms before DFT) | Capture 360° of cogging Iq/Id |
| **4. Return** | Centering | Variable | PID Control (Position Ramp to 0.0) | Unwind motor revolutions |
| | Final Align | - | `EncoderInit` State (`bangInitEnc`) | Reset hardware alignment |

| **3. Acquisition** | Setup | 1000ms | Zero Torque | Settle motor at rest |
| | DFT Integration | ~10.5s | PID Control (Wait 1500ms before DFT) | Capture 360° of cogging Iq/Id |
| **4. Return** | Centering | Variable | PID Control (Position Ramp to 0.0) | Unwind motor revolutions |
| | Final Align | - | `EncoderInit` State (`bangInitEnc`) | Reset hardware alignment |

### Configuration Macros
*   `COGGING_CALIB_LUT_RESOLUTION`: Standardized at 2880 points for communication protocol compatibility.
*   `COGGING_CALIB_TIME_PER_REV_S`: Time in seconds to complete one revolution (Default: 8s).
*   `COGGING_CALIB_DFT_HARMONICS`: Number of analyzed harmonics (128).
*   `COGGING_CALIB_ENABLE_ID_DIAG`: Macro to enable electrical phase analysis on the Id axis.

## 4. Design Choices & Timing Analysis

### Why 8 Seconds per Revolution?
This speed (**7.5 RPM**) is carefully selected as a "Physical Sweet Spot":
*   **Avoiding Stick-Slip**: It is fast enough to ensure the motor remains in the "viscous friction" regime, avoiding the jerky "stick-slip" motion caused by static friction.
*   **Negligible Dynamics**: It is slow enough that Back-EMF, rotor inertia, and phase lag are negligible. The torque measured is almost purely the sum of friction and cogging.

### Why 1kHz Sampling & Control?
The choice of a synchronous 1kHz loop for both the PID and DFT is driven by signal integrity and temporal alignment:
*   **Synchronous SPI**: The system uses blocking synchronous register writes (`writeReg`) instead of DMA/Async to ensure that torque commands and position samples remain strictly aligned in time within the 1ms window.
*   **Encoder Quantization**: At this speed, a standard encoder (e.g., 65k CPR) provides ~8 transitions per millisecond. Sampling faster (e.g., 10kHz) would result in many samples with 0 or 1 transitions, creating massive noise in the PID's derivative term ($K_d$) and polluting the DFT.
*   **Nyquist Margin**: For 128 harmonics at 0.125Hz, the highest frequency of interest is ~16Hz. 1kHz provides an oversampling ratio of **60x**, ensuring excellent anti-aliasing.
*   **Temporal Averaging**: By integrating 1kHz samples into the DFT, we perform a natural temporal average of the torque on each encoder quantization step, which significantly cleans the measurement before harmonic extraction.

### Multi-Rate Evaluation (4kHz Control vs 1kHz Acquisition)
While a 4kHz PID loop would theoretically offer higher control bandwidth, it was rejected for this specific task because:
1.  **Encoder Noise**: At 7.5 RPM, the increased bandwidth would mostly react to encoder quantization noise rather than actual cogging.
2.  **System Load**: 4kHz would quadruple the SPI bus traffic and CPU interrupts, potentially impacting USB HID latency or other real-time tasks, with no measurable gain in calibration accuracy.

## 4. Diagnostics & Engineering Points

### Point 1: Electrical Diagnostic (`COGGING_CALIB_ENABLE_ID_DIAG`)
*   Analyzes the **Id axis** (Flux).
*   In a healthy motor, the flux remains DC. Significant energy in **H3** or **H6** indicates a phase imbalance or partial winding short.

### Point 2: Mechanical Diagnostic (Eccentricity)
*   Analyzes the **H1** magnitude on the Iq axis.
*   A high H1 magnitude signals rotor eccentricity, a bent shaft, or encoder misalignment.

### Point 4: Harmonic Anti-Cogging
*   The system scans all 128 harmonics.
*   It selects the **Top 20 peaks** with an **Order > 10**.
*   This captures the high-frequency magnetic "detent" while ignoring the low-frequency gravitational imbalance of asymmetric steering wheels.

## 5. Real-Time Compensation Formula

Implemented in the `turn()` method for zero latency:

$$T_{comp}(\theta) = \sum_{n=1}^{20} A_n \cdot \sin(Order_n \cdot \theta + \Phi_n)$$
$$T_{final} = T_{requested} + T_{comp}(\theta)$$
