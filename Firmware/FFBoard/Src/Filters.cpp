/*
 * Filters.cpp
 *
 *  Created on: Feb 13, 2020
 *      Author: Yannick
 */

#include "Filters.h"

#include <math.h>

InterpFFB::InterpFFB(){
	interp_f = 0.0;
}

InterpFFB::InterpFFB(int16_t ifactor){
	setInterpFactor(ifactor);
}

InterpFFB::~InterpFFB() {
}

void InterpFFB::setInterpFactor(int16_t ifactor){
    this->interp_f = ifactor;
}

int16_t InterpFFB::getInterpFactor(){
    return this->interp_f;
}

int16_t InterpFFB::getEffectiveInterpFactor(){
    return this->effective_interp_f;
}
float InterpFFB::lerp(float a, float b, float c){
        return a + (b - a) * c;
}

//Note: This function is called in a way that effective_interp_f can never be 1 or 0.
//It minimum can be 2. If it is 1 or 0, this filter is not activated.
float InterpFFB::interpFloat(int32_t input, uint32_t auto_interp_factor){
                    if(auto_interp_factor != 1){
                        effective_interp_f = auto_interp_factor;
                    }
                    else {
                        effective_interp_f = interp_f;
                    }
					
                    if(input_backup != input) {
                        cd = input_backup;
                        input_backup = input;
                        lerpCount = 0;
                    }
                    if(lerpCount < effective_interp_f) {
                    lerpCount++;
                    return lerp((float)cd,(float)input, ((float)lerpCount / (float)effective_interp_f));
                    } else {
                    return (float)input;
                    }
}

Biquad::Biquad(){
	z1 = z2 = 0.0;
}
Biquad::Biquad(BiquadType type, float Fc, float Q, float peakGainDB) {
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

float Biquad::getFc() {
	return this->Fc;
}

/**
 * Changes Q value and recalculaes filter
 */
void Biquad::setQ(float Q) {
    this->Q = Q;
    calcBiquad();
}

float Biquad::getQ() {
	return this->Q;
}

/**
 * Calculates one step of the filter and returns the output
 */
float Biquad::process(float in) {
	float out = in * a0 + z1;
    z1 = in * a1 + z2 - b1 * out;
    z2 = in * a2 - b2 * out;
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
	z1 = 0.0;
	z2 = 0.0;
    float norm;
    float V = pow(10, fabs(peakGain) / 20.0);
    float K = tan(M_PI * Fc);
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
                norm = 1 / (1 + sqrt(2) * K + K * K);
                a0 = (1 + sqrt(2*V) * K + V * K * K) * norm;
                a1 = 2 * (V * K * K - 1) * norm;
                a2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - sqrt(2) * K + K * K) * norm;
            }
            else {    // cut
                norm = 1 / (1 + sqrt(2*V) * K + V * K * K);
                a0 = (1 + sqrt(2) * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - sqrt(2) * K + K * K) * norm;
                b1 = 2 * (V * K * K - 1) * norm;
                b2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
            }
            break;
        case BiquadType::highshelf:
            if (peakGain >= 0) {    // boost
                norm = 1 / (1 + sqrt(2) * K + K * K);
                a0 = (V + sqrt(2*V) * K + K * K) * norm;
                a1 = 2 * (K * K - V) * norm;
                a2 = (V - sqrt(2*V) * K + K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - sqrt(2) * K + K * K) * norm;
            }
            else {    // cut
                norm = 1 / (V + sqrt(2*V) * K + K * K);
                a0 = (1 + sqrt(2) * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - sqrt(2) * K + K * K) * norm;
                b1 = 2 * (K * K - V) * norm;
                b2 = (V - sqrt(2*V) * K + K * K) * norm;
            }
            break;
    }

    return;
}
