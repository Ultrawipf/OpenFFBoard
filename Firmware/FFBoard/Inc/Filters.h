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


#endif

#endif /* FILTERS_H_ */
