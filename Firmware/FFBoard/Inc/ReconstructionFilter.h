/*
 * ReconstructionFilter.h
 *
 *  Created on: 2026-06-06
 *      Author: Antigravity / @safety-watchdog-agent
 *
 *  Class encapsulating reconstruction filtering logic using spline interpolation.
 */

#ifndef RECONSTRUCTION_FILTER_H_
#define RECONSTRUCTION_FILTER_H_

#include "cppmain.h"

#ifdef USE_DSP_FUNCTIONS
#include "arm_math_types.h"
#include "dsp/interpolation_functions.h"
#endif

enum class ReconFilterMode : uint8_t {
	NO_RECONSTRUCTION = 0,
    LINEAR_INTERPOLATION = 1, 	// More responsive, attempts to match the game's signal precisely. Can feel "grainy" if slew rate is high.
	SPLINE_CUBIC_NATURAL = 2,	// Highest fidelity, smooth curve through last 4 points. More CPU intensive.
    SPLINE_CUBIC_HERMITE = 3  	// Mixed solution, good fidelity and optimized timing
};

class ReconstructionFilter {
public:
    ReconstructionFilter();
    ~ReconstructionFilter() = default;

    /**
     * @brief Push a new sample to the spline buffers.
     * @param newValue The new value to filter.
     */
    void push(float newValue);

    /**
     * @brief Evaluate the reconstruction filter.
     * @param fallbackValue The default value to return if the filter is not ready.
     * @param mode The reconstruction filter mode.
     * @return The interpolated/reconstructed value.
     */
    float evaluate(float fallbackValue, ReconFilterMode mode);

private:
#ifdef USE_DSP_FUNCTIONS
    float32_t spline_x[4];             // Buffer time (in us)
    float32_t spline_y[4];             // Buffer value
    float32_t spline_y2[4];            // Buffer for Spline Natural
    float32_t spline_scratch[8];       // Buffer for Spline Natural
    arm_spline_instance_f32 spline_instance; // Instance for CMSIS-DSP
    bool spline_arm_initialized;       // CMSIS-DSP initialized ?
#else
    float spline_x[4];                 // Buffer time (in us)
    float spline_y[4];                 // Buffer value
#endif
    bool isSplineReady;                // Buffer is full ?
};

#endif /* RECONSTRUCTION_FILTER_H_ */
