/*
 * Filters.h
 *
 *  Created on: Feb 13, 2020
 *      Author: Yannick
 */

#ifndef FILTERS_H_
#define FILTERS_H_
#include "cppmain.h"

#ifdef __cplusplus

struct biquad_constant_t {
	uint16_t freq;
	uint8_t q;
};

enum class BiquadType : uint8_t {
    lowpass = 0,
    highpass,
    bandpass,
    notch,
    peak,
    lowshelf,
    highshelf
};

class TMC4671Biquad;
class Biquad{
	friend TMC4671Biquad;
public:
	Biquad();
    Biquad(BiquadType type, float Fc, float Q, float peakGainDB);
    ~Biquad();
    float process(float in);
    void setBiquad(BiquadType type, float Fc, float Q, float peakGain);
    void setFc(float Fc); //frequency
    float getFc();
    void setQ(float Q);
    float getQ();
    void calcBiquad(void);

protected:

    BiquadType type;
    float a0, a1, a2, b1, b2;
    float Fc, Q, peakGain;
    float z1, z2;
};

class InterpFFB {
public:

	InterpFFB();
	InterpFFB(int16_t interp_f);
    ~InterpFFB();
    void setInterpFactor(int16_t interp_f);
    int16_t getInterpFactor();
    int16_t getEffectiveInterpFactor();
	float interpFloat(int32_t input, uint32_t auto_interp_factor);
    float lerp(float a, float b, float c);

protected:

	int16_t interp_f = 0; //interp factor
    float cd = 0.0; //current
    int32_t input_backup = 0.0; //to track goal from a step behind
	float dd = 0.0; //delta
    int16_t effective_interp_f = 0;
    int8_t lerpCount = 0;
};

#endif

#endif /* FILTERS_H_ */
