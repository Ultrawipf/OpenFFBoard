/*
 * Filters.cpp
 *
 *  Created on: Feb 13, 2020
 *      Author: Yannick, Vincent
 *
 *  Based on http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/
 */

#include "Filters.h"
#include <math.h>

#ifdef USE_DSP_FUNCTIONS
#include "arm_math.h"
#endif

Biquad::Biquad(){
#ifdef USE_DSP_FUNCTIONS
    // Clear state
    memset(pState, 0, sizeof(pState));
	// Initialize the CMSIS-DSP biquad instance
    arm_biquad_cascade_df1_init_f32(&S, 1, pCoeffs, pState);
#else
    z1 = 0.0f;
    z2 = 0.0f; 
#endif
}
Biquad::Biquad(BiquadType type, float Fc, float Q, float peakGainDB) {
#ifdef USE_DSP_FUNCTIONS
    arm_biquad_cascade_df1_init_f32(&S, 1, pCoeffs, pState);
#endif
    setBiquad(type, Fc, Q, peakGainDB);
}

Biquad::~Biquad() {
}

/**
 * Sets the frequency
 * Calculate as Fc = f/samplerate
 * Must be lower than 0.5
 */
void Biquad::setFc(float Fc) {
	Fc = clip<float,float>(Fc,0,0.5);
    this->Fc = Fc;
    calcBiquad();
}

float Biquad::getFc() const {
	return this->Fc;
}

/**
 * Changes Q value and recalculaes filter
 */
void Biquad::setQ(float Q) {
    this->Q = Q;
    calcBiquad();
}

float Biquad::getQ() const {
	return this->Q;
}

void Biquad::setPeakGain(float peakGainDB) {
    this->peakGain = peakGainDB;
    calcBiquad();
}

/**
 * Calculates one step of the filter and returns the output
 */
float Biquad::process(float in) {
    float out;
#ifdef USE_DSP_FUNCTIONS
    arm_biquad_cascade_df1_f32(&S, &in, &out, 1);
#else
	out = in * a0 + z1;
    z1 = in * a1 + z2 - b1 * out;
    z2 = in * a2 - b2 * out;
#endif
    return out;
}

void Biquad::setBiquad(BiquadType type, float Fc, float Q, float peakGainDB) {
	Fc = clip<float,float>(Fc,0,0.5);
    this->type = type;
    this->Q = Q;
    this->Fc = Fc;
    this->peakGain = peakGainDB;
    calcBiquad();
}

/*
 * Updates parameters and resets the biquad filter
 */
void Biquad::calcBiquad(void) {
    float norm;
    float K, V;
#ifdef USE_DSP_FUNCTIONS
    float a0, a1, a2, b1, b2;
    V = powf(10, fabsf(peakGain) / 20.0f);
    K = arm_sin_f32(PI * Fc) / arm_cos_f32(PI * Fc);
#else
	z1 = 0.0;
	z2 = 0.0;
    V = pow(10, fabs(peakGain) / 20.0);
    K = tan(M_PI * Fc);
#endif
    switch (this->type) {
        case BiquadType::lowpass:
            norm = 1 / (1 + K / Q + K * K);
            a0 = K * K * norm;
            a1 = 2 * a0;
            a2 = a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case BiquadType::highpass:
            norm = 1 / (1 + K / Q + K * K);
            a0 = 1 * norm;
            a1 = -2 * a0;
            a2 = a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case BiquadType::bandpass:
            norm = 1 / (1 + K / Q + K * K);
            a0 = K / Q * norm;
            a1 = 0;
            a2 = -a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case BiquadType::notch:
            norm = 1 / (1 + K / Q + K * K);
            a0 = (1 + K * K) * norm;
            a1 = 2 * (K * K - 1) * norm;
            a2 = a0;
            b1 = a1;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case BiquadType::peak:
            if (peakGain >= 0) {    // boost
                norm = 1 / (1 + 1/Q * K + K * K);
                a0 = (1 + V/Q * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - V/Q * K + K * K) * norm;
                b1 = a1;
                b2 = (1 - 1/Q * K + K * K) * norm;
            }
            else {    // cut
                norm = 1 / (1 + V/Q * K + K * K);
                a0 = (1 + 1/Q * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - 1/Q * K + K * K) * norm;
                b1 = a1;
                b2 = (1 - V/Q * K + K * K) * norm;
            }
            break;
        case BiquadType::lowshelf:
            if (peakGain >= 0) {    // boost
				float sqrt2, sqrt2V;
#ifdef USE_DSP_FUNCTIONS
				arm_sqrt_f32(2, &sqrt2);
				arm_sqrt_f32(2*V, &sqrt2V);
#else
                sqrt2 = sqrt(2);
                sqrt2V = sqrt(2*V);
#endif
                norm = 1 / (1 + sqrt2 * K + K * K);
                a0 = (1 + sqrt2V * K + V * K * K) * norm;
                a1 = 2 * (V * K * K - 1) * norm;
                a2 = (1 - sqrt2V * K + V * K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - sqrt2 * K + K * K) * norm;
            }
            else {    // cut
				float sqrt2, sqrt2V;
#ifdef USE_DSP_FUNCTIONS
				arm_sqrt_f32(2, &sqrt2);
				arm_sqrt_f32(2*V, &sqrt2V);
#else
                sqrt2 = sqrt(2);
                sqrt2V = sqrt(2*V);
#endif
                norm = 1 / (1 + sqrt2V * K + V * K * K);
                a0 = (1 + sqrt2 * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - sqrt2 * K + K * K) * norm;
                b1 = 2 * (V * K * K - 1) * norm;
                b2 = (1 - sqrt2V * K + V * K * K) * norm;
            }
            break;
        case BiquadType::highshelf:
            if (peakGain >= 0) {    // boost
				float sqrt2, sqrt2V;
#ifdef USE_DSP_FUNCTIONS
				arm_sqrt_f32(2, &sqrt2);
				arm_sqrt_f32(2*V, &sqrt2V);
#else
                sqrt2 = sqrt(2);
                sqrt2V = sqrt(2*V);
#endif
                norm = 1 / (1 + sqrt2 * K + K * K);
                a0 = (V + sqrt2V * K + K * K) * norm;
                a1 = 2 * (K * K - V) * norm;
                a2 = (V - sqrt2V * K + K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - sqrt2 * K + K * K) * norm;
            }
            else {    // cut
				float sqrt2, sqrt2V;
#ifdef USE_DSP_FUNCTIONS
				arm_sqrt_f32(2, &sqrt2);
				arm_sqrt_f32(2*V, &sqrt2V);
#else
                sqrt2 = sqrt(2);
                sqrt2V = sqrt(2*V);
#endif
                norm = 1 / (V + sqrt2V * K + K * K);
                a0 = (1 + sqrt2 * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - sqrt2 * K + K * K) * norm;
                b1 = 2 * (K * K - V) * norm;
                b2 = (V - sqrt2V * K + K * K) * norm;
            }
            break;
    }

#ifdef USE_DSP_FUNCTIONS
    // Store coefficients in the format required by CMSIS-DSP: {b0, b1, b2, -a1, -a2}
    // Note the negated feedback coefficients a1 and a2.
    pCoeffs[0] = a0;
    pCoeffs[1] = a1;
    pCoeffs[2] = a2;
    pCoeffs[3] = -b1;
    pCoeffs[4] = -b2;

	// Reset state
	memset(pState, 0, sizeof(pState));
#endif
}
