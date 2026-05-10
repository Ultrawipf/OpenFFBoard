# Anti-Cogging and Motor Diagnostic Analysis (Fourier Method)

This document describes the implementation of the harmonic-based anti-cogging compensation and motor diagnostic system for the OpenFFBoard (TMC4671).

## 1. Process Overview

The calibration process replaces the legacy lookup table (LUT) with a real-time Fourier Series synthesis. It follows a dual-pass acquisition method:

1.  **Dual-Pass Acquisition**: The motor is rotated at a constant slow speed (**8 RPM**) in both forward and backward directions.
    *   **Forward Pass**: Captures `Torque_Target + Friction + Cogging`.
    *   **Backward Pass**: Captures `-Torque_Target - Friction + Cogging`.
2.  **Spatio-Temporal Sampling**: Data is sampled at the maximum CPU frequency. Positions are mapped to **1024 spatial bins** per mechanical revolution.
3.  **FFT Analysis**: A Fast Fourier Transform (FFT) is performed using the CMSIS-DSP library to extract the magnitude and phase of each spatial harmonic.
4.  **Harmonic Synthesis**: The top 20 most significant harmonics are stored and used in the real-time torque loop to inject an opposing current.

## 2. Technical Specifications

### Why 1024 Positions?
*   **Spatial Resolution**: 1024 bins provide a resolution of **0.35°** per bin.
*   **Nyquist Limit**: This allows the detection of up to the **512th harmonic**. Even for a high pole-count motor (e.g., 24 poles) with a high slot count, the main cogging frequency (PPCM of poles/slots) rarely exceeds 200 cycles per revolution.
*   **Memory Efficiency**: A `float32` array of 1024 points occupies only **4 KB**. This is the "sweet spot" to avoid the 17 KB allocation crash while maintaining extreme precision.

### Why 20 Harmonics?
*   **Signal Purity**: Cogging is never a perfect sine wave; it is a complex periodic signal. Summing 20 sine waves allows for the reconstruction of the "shape" of the cogging pulse (including its higher-order resonances) while ignoring random measurement noise.
*   **Performance**: The Cortex-M4F FPU can calculate 20 `arm_sin_f32` operations in a few microseconds, which is negligible in a 1 kHz FFB loop.
*   **Storage**: 20 harmonics (Amplitude, Phase, Order) take only **200 bytes** in Flash, compared to 5.7 KB for the legacy table.

## 3. Detected Harmonics & Diagnostics

The system performs automated diagnostics during calibration to address three key engineering points:

### Point 1: Electrical Fault Detection (Phase Imbalance)
*   **Signal**: Analysis of the **Actual Flux ($I_d$)**.
*   **Harmonics**: Focus on **H3** and **H6**.
*   **Logic**: In a balanced 3-phase motor, the flux axis should remain steady at 0. Peaks at H3 or H6 indicate a partial short circuit between windings or an electrical imbalance in the driver.

### Point 2: Mechanical Diagnostic (Eccentricity)
*   **Signal**: Analysis of the **Target Torque ($I_q$)**.
*   **Harmonics**: Focus on **H1** (Fundamental mechanical frequency).
*   **Logic**: A large H1 magnitude indicates that the rotor is off-center (eccentricity), the shaft is bent, or the encoder magnet is misaligned (runout).

### Point 4: Anti-Cogging (Torque Ripple)
*   **Signal**: Analysis of the **Target Torque ($I_q$)**.
*   **Harmonics**: Selection of the **Top 20 peaks** with an **Order > 10**.
*   **Logic**: Orders below 11 are intentionally ignored to avoid compensating for the natural gravitational imbalance of non-round steering wheels (e.g., GT or Formula rims). Orders above 10 capture the true magnetic cogging.

## 4. Real-Time Compensation Formula

The compensated torque ($T_{final}$) is calculated in the `turn()` method:

$$T_{comp}(\theta) = \sum_{n=1}^{20} A_n \cdot \sin(Order_n \cdot \theta + \Phi_n)$$
$$T_{final} = T_{requested} - T_{comp}(\theta)$$

Where:
*   $A_n$: Amplitude of harmonic $n$.
*   $\Phi_n$: Phase shift of harmonic $n$.
*   $Order_n$: Spatial frequency (cycles/rev).
*   $\theta$: Mechanical angle in radians $[0, 2\pi]$.
