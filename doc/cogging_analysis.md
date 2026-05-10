# Anti-Cogging and Motor Diagnostic Analysis (Continuous DFT-128 Method)

This document describes the implementation of the harmonic-based anti-cogging compensation and motor diagnostic system for the OpenFFBoard (TMC4671).

## 1. Process Overview

The system uses a **Continuous Discrete Fourier Transform (DFT)** integration. It eliminates lookup tables and fixed-size buffers, making it memory-safe for all supported microcontrollers (F407 and F411).

1.  **Software PID & Auto-Tuning**: Before acquisition, the system performs a dynamic auto-tuning of a software PID controller (using CMSIS-DSP). It identifies the motor's static friction and uses the **Relay Feedback method** ([reference](https://en.wikipedia.org/wiki/Relay_feedback_test)) and **Ziegler-Nichols method** ([reference](https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method)) to calculate optimal gains.
    *   **Initial Capture**: The system starts oscillating to determine the base PID parameters for the velocity controller.
    *   **Dynamic Profiling**: The system analyzes the oscillation period ($T_u$) to automatically differentiate between **Small/Low-Inertia motors** (NEMA17, Gimbal) and **Large/High-Inertia motors** (MiGE 130ST). It applies specific gain scalers to ensure stability and high-bandwidth response across the entire motor range.
    *   **Validation & Elastic Fine-Tuning**: A high-precision verification phase (aiming for **0.1 degree** of tracking error) is performed. The system runs a 600ms test rotation; if the target precision isn't met, it iteratively "boosts" the PID stiffness (up to 5 attempts) while adaptively relaxing the tolerance if the mechanical setup limits precision.
1.  **Torque Response Capture (Deterministic Dual-Pass Acquisition)**: The motor rotates at a constant speed defined by **COGGING_CALIB_TIME_PER_REV_S** (default: 8 seconds per revolution) in both directions.
    *   **1kHz Strict Integration**: Acquisition is strictly clocked at 1kHz. For an 8-second tour, exactly **8,000 samples** are integrated. This ensures perfect spatial alignment and mathematical precision for the DFT (detailed below).
    *   **Friction Feed-Forward**: The breakaway friction torque discovered during auto-tuning is applied as a **Feed-Forward** ([reference](https://en.wikipedia.org/wiki/Feed_forward_(control))) base torque. This allows the motor to reach a constant velocity instantly, eliminating start-up transients in the DFT data.
    *   **Actual Current Feedback**: The system integrates the **actual currents (Iq/Id)** read from the TMC hardware registers, rather than the controller's command. This captures the real physical interaction between the motor and the magnetic cogging.
    *   **Complex Rotation Optimization**: Uses recursive complex multiplication ($e^{i(k+1)\theta} = e^{ik\theta} \cdot e^{i\theta}$) to calculate 128 harmonics with only one trigonometric call per sample.

## 2. Technical Specifications

### Memory Efficiency (The Zero-Buffer Goal)
*   **Accumulators**: Uses 128 pairs of `float` (32-bit) values to leverage the hardware FPU for high-performance integration.
*   **Peak RAM**: Approximately **2 KB** on the FreeRTOS heap during calibration.
*   **Permanent RAM**: Only **240 bytes** (20 saved harmonics).
*   **Compatibility**: Fully portable between F407 and F411.

### Configuration Macros
*   `COGGING_CALIB_LUT_RESOLUTION`: Standardized at 2880 points for communication protocol compatibility.
*   `COGGING_CALIB_TIME_PER_REV_S`: Time in seconds to complete one revolution (Default: 8s).
*   `COGGING_CALIB_DFT_HARMONICS`: Number of analyzed harmonics (128).
*   `COGGING_CALIB_ENABLE_ID_DIAG`: Macro to enable electrical phase analysis on the Id axis.

## 3. Design Choices & Timing Analysis

### Why 8 Seconds per Revolution?
This speed (**7.5 RPM**) is carefully selected as a "Physical Sweet Spot":
*   **Avoiding Stick-Slip**: It is fast enough to ensure the motor remains in the "viscous friction" regime, avoiding the jerky "stick-slip" motion caused by static friction at extremely low speeds.
*   **Negligible Dynamics**: It is slow enough that Back-EMF, rotor inertia, and phase lag are negligible. The torque measured is almost purely the sum of friction and cogging.

### Why 1kHz Sampling & Control?
The choice of a synchronous 1kHz loop for both the PID and DFT is driven by signal integrity:
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
*   This captures the high-frequency magnetic "detent" while ignoring the low-frequency gravitational imbalance of asymmetric (GT/Formula) steering wheels.

## 4. Real-Time Compensation Formula

Implemented in the `turn()` method for zero latency:

$$T_{comp}(\theta) = \sum_{n=1}^{20} A_n \cdot \sin(Order_n \cdot \theta + \Phi_n)$$
$$T_{final} = T_{requested} - T_{comp}(\theta)$$
