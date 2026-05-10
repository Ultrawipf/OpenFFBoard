# Anti-Cogging and Motor Diagnostic Analysis (Continuous DFT-128 Method)

This document describes the final implementation of the harmonic-based anti-cogging compensation and motor diagnostic system for the OpenFFBoard (TMC4671).

## 1. Process Overview

The system uses a **Continuous Discrete Fourier Transform (DFT)** integration. It eliminates lookup tables and fixed-size buffers, making it memory-safe for all supported microcontrollers (F407 and F411).

1.  **Dual-Pass Acquisition**: The motor rotates at **COGGING_CALIB_SPEED_RPM** (default: 8 RPM) in both directions.
2.  **Full-Speed Integration**: Every sample provided by the CPU is processed. At typical SPI speeds, the system integrates ~150,000 samples per tour.
3.  **Complex Rotation Optimization**: Uses recursive complex multiplication ($e^{i(k+1)\theta} = e^{ik\theta} \cdot e^{i\theta}$) to calculate 128 harmonics with only one trigonometric call per sample.
4.  **Mathematical Extraction**: At the end of the tour, the 128 mathematical harmonics are analyzed to identify the motor geometry.

## 2. Technical Specifications

### Memory Efficiency (The Zero-Buffer Goal)
*   **Accumulators**: Uses 128 pairs of `double` (64-bit) values to prevent rounding errors.
*   **Peak RAM**: Only **4 KB** on the FreeRTOS heap during calibration.
*   **Permanent RAM**: Only **240 bytes** (20 saved harmonics).
*   **Compatibility**: Fully portable between F407 and F411.

### Configuration Macros
*   `COGGING_CALIB_LUT_RESOLUTION`: Standardized at 2880 points for communication protocol compatibility.
*   `COGGING_CALIB_SPEED_RPM`: Rotation speed (8 RPM).
*   `COGGING_CALIB_DFT_HARMONICS`: Number of analyzed harmonics (128).
*   `COGGING_CALIB_ENABLE_ID_DIAG`: Macro to enable electrical phase analysis on the Id axis.

## 3. Diagnostics & Engineering Points

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
