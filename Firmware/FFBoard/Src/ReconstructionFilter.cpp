/*
 * ReconstructionFilter.cpp
 *
 *  Created on: 2026-06-06
 *      Author: Antigravity / @safety-watchdog-agent
 *
 *  Class implementing reconstruction filtering logic using spline interpolation.
 */

#include "ReconstructionFilter.h"
#include "cppmain.h"

ReconstructionFilter::ReconstructionFilter() {
#ifdef USE_DSP_FUNCTIONS
    for (int i = 0; i < 4; ++i) {
        spline_x[i] = 0.0f;
        spline_y[i] = 0.0f;
        spline_y2[i] = 0.0f;
    }
    for (int i = 0; i < 8; ++i) {
        spline_scratch[i] = 0.0f;
    }
    spline_arm_initialized = false;
#else
    for (int i = 0; i < 4; ++i) {
        spline_x[i] = 0.0f;
        spline_y[i] = 0.0f;
    }
#endif
    isSplineReady = false;
}

void ReconstructionFilter::push(float newValue) {
    uint32_t now_us = micros();
    for (int i = 0; i < 3; ++i) {
        spline_x[i] = spline_x[i + 1];
        spline_y[i] = spline_y[i + 1];
    }
    spline_x[3] = (float)now_us;
    spline_y[3] = newValue;
    
    if (!isSplineReady) {
        float fake_time_step = 16666.0f; // 60Hz
        spline_x[0] = spline_x[3] - 3.0f * fake_time_step;
        spline_y[0] = spline_y[3];
        spline_x[1] = spline_x[3] - 2.0f * fake_time_step;
        spline_y[1] = spline_y[3];
        spline_x[2] = spline_x[3] - 1.0f * fake_time_step;
        spline_y[2] = spline_y[3];
        isSplineReady = true; 
    }

#ifdef USE_DSP_FUNCTIONS
    arm_spline_init_f32(&spline_instance, ARM_SPLINE_NATURAL, spline_x, spline_y, 4, spline_y2, spline_scratch);
    spline_arm_initialized = true;
#endif
}

float ReconstructionFilter::evaluate(float fallbackValue, ReconFilterMode mode) {
    float resulting_value = fallbackValue;
    uint32_t now_us = micros();
    float last_known_time = spline_x[3];

    if (!isSplineReady) return fallbackValue;
    if (now_us > last_known_time + 50000) return spline_y[3];

    switch(mode) {
        case ReconFilterMode::NO_RECONSTRUCTION:
            resulting_value = spline_y[3];
            break;
        case ReconFilterMode::LINEAR_INTERPOLATION: {
            float t0 = spline_x[2];
            float t1 = spline_x[3];
            float v0 = spline_y[2];
            float v1 = spline_y[3];
            float expected_interval = t1 - t0;
            if (expected_interval < 1.0f) {
                resulting_value = v1;
                break;
            }
            float time_since_t1 = (float)(now_us - t1);
            float progress = clip<float>(time_since_t1 / expected_interval, 0.0f, 1.0f);
            resulting_value = v0 + progress * (v1 - v0);
            break;
        }
#ifdef USE_DSP_FUNCTIONS
        case ReconFilterMode::SPLINE_CUBIC_NATURAL: {
            if (!spline_arm_initialized) {
                resulting_value = spline_y[3];
                break;
            }
            float32_t interpolated_torque_f = 0.0f;
            float32_t now_f = (float)now_us;
            float32_t interp_time = clip<float32_t>(now_f, spline_x[0], spline_x[3]);
            arm_spline_f32(&spline_instance, &interp_time, &interpolated_torque_f, 1);
            resulting_value = interpolated_torque_f;
            break;
        }
#endif
        case ReconFilterMode::SPLINE_CUBIC_HERMITE:
#ifndef USE_DSP_FUNCTIONS
        case ReconFilterMode::SPLINE_CUBIC_NATURAL:
#endif
        {
            const float p1 = spline_y[1];
            const float p2 = spline_y[2];
            const float t1 = spline_x[1];
            const float t2 = spline_x[2];
            float interval = t2 - t1;
            if (interval <= 0) {
                resulting_value = p1;
                break;
            }
            float t = clip<float>(((float)now_us - t1) / interval, 0.0f, 1.0f);
            float dt_m1 = spline_x[2] - spline_x[0];
            float dt_m2 = spline_x[3] - spline_x[1];
            float m1 = (dt_m1 > 0) ? ((spline_y[2] - spline_y[0]) / dt_m1) * interval : 0;
            float m2 = (dt_m2 > 0) ? ((spline_y[3] - spline_y[1]) / dt_m2) * interval : 0;
            float tSq = t * t;
            float tCub = tSq * t;
            resulting_value = (2*tCub - 3*tSq + 1) * p1 + (tCub - 2*tSq + t) * m1 + (-2*tCub + 3*tSq) * p2 + (tCub - tSq) * m2;
            break;
        }
    }
    return resulting_value;
}
