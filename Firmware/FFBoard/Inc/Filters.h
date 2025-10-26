/*
 * Filters.h
 *
 *  Created on: Feb 13, 2020
 *      Author: Yannick, Vincent
 */

#ifndef FILTERS_H_
#define FILTERS_H_
#include "cppmain.h"
#include "arm_math.h"

#ifdef __cplusplus

// Frequency in hz, q in float q*100. Example: Q 0.5 -> 50
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
    float getFc() const;
    void setQ(float Q);
    float getQ() const;
    void setPeakGain(float peakGainDB);
    void calcBiquad(void);

    const float* getCoeffs() const { return pCoeffs; }

protected:
    BiquadType type;
    float Fc, Q, peakGain;

    // CMSIS-DSP instance
    arm_biquad_casd_df1_inst_f32 S;
    float32_t pCoeffs[5];
    float32_t pState[4]; // For a single biquad stage (DF1 float requires 4 states)
};


#endif

#endif /* FILTERS_H_ */
