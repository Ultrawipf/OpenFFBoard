/*
 * Effect.h
 *
 *  Created on: 2026-05-29
 *      Author: Antigravity
 */

#ifndef EFFECT_H_
#define EFFECT_H_

#include "ffb_defs.h"
#include "Filters.h"
#include <memory>
#include <algorithm>
#include <cmath>
#include <vector>

#define EFFECT_STATE_INACTIVE 0

struct metric_t;

// Forward declarations if needed
class Axis;

// =======================================================================
// BASE CLASS : Effect
// =======================================================================
class Effect {
protected:
    uint8_t type = FFB_EFFECT_NONE;
    volatile uint8_t state = 0; // EFFECT_STATE_INACTIVE
    uint32_t duration = FFB_EFFECT_DURATION_INFINITE;
    uint32_t startTime = 0;
    uint16_t startDelay = 0;
    uint8_t gain = 255;
    float axisMagnitudes[MAX_AXIS] = {0};
    bool useSingleCondition = true;
    uint16_t samplePeriod = 0;
    
    std::unique_ptr<Biquad> filter[MAX_AXIS] = { nullptr };

    virtual int32_t calculateRawForce(uint8_t axis, metric_t* metrics) = 0;

public:
    virtual ~Effect() = default;

    uint8_t getType() const { return type; }
    uint8_t getState() const { return state; }
    void setState(uint8_t s) { state = s; }
    void setStartTime(uint32_t t) { startTime = t; }
    uint32_t getStartTime() const { return startTime; }
    uint16_t getStartDelay() const { return startDelay; }
    void setGain(uint8_t g) { gain = g; }
    void setDuration(uint32_t d) { duration = d; }
    uint32_t getDuration() const { return duration; }
    void setStartDelay(uint16_t d) { startDelay = d; }
    void setAxisMagnitude(uint8_t axis, float mag) { axisMagnitudes[axis] = mag; }
    float getAxisMagnitude(uint8_t axis) const { return axisMagnitudes[axis]; }
    void setUseSingleCondition(bool single) { useSingleCondition = single; }
    bool getUseSingleCondition() const { return useSingleCondition; }

    void setFilter(uint8_t axis, float freq, float q, float peakGain = 0.0) {
        if (!filter[axis]) filter[axis] = std::make_unique<Biquad>(BiquadType::lowpass, freq, q, peakGain);
        else filter[axis]->setBiquad(BiquadType::lowpass, freq, q, peakGain);
    }
    Biquad* getFilter(uint8_t axis) { return filter[axis].get(); }
    void setSamplePeriod(uint16_t period) { samplePeriod = period; }

    int32_t processForce(uint8_t axis, metric_t* metrics, uint8_t global_gain);

    virtual void setFilters(float calcfrequency, uint8_t profileId) {}

    virtual void setEnvelope(FFB_SetEnvelope_Data_t* report) {}
    virtual void setPeriodic(FFB_SetPeriodic_Data_t* report) {}
    virtual void setCondition(FFB_SetCondition_Data_t* report) {}
    virtual void setRamp(FFB_SetRamp_Data_t* report) {}
    virtual void setConstantForce(FFB_SetConstantForce_Data_t* report) {}
    
    // Some general manipulation methods needed by SerialFFB
    virtual void setMagnitude(int16_t mag) {}
    virtual int16_t getMagnitude() const { return 0; }
    virtual void setOffset(int16_t off) {}
    virtual int16_t getOffset() const { return 0; }
    virtual void setPeriod(uint32_t p) {}
    virtual uint32_t getPeriod() const { return 0; }
    
    virtual FFB_Effect_Condition* getCondition(uint8_t axis) { return nullptr; }
};

// =======================================================================
// BRANCH 1 : EffectTemporal (Basés sur le temps)
// =======================================================================
class EffectTemporal : public Effect {
protected:
    int16_t magnitude = 0;
    int16_t offset = 0;
    int16_t phase = 0;
    uint32_t period = 0;

    bool useEnvelope = false;
    uint16_t attackLevel = 0;
    uint16_t fadeLevel = 0;
    uint32_t attackTime = 0;
    uint32_t fadeTime = 0;

    ReconFilterState recon_magnitude;
    ReconFilterState recon_offset;

    int32_t getEnvelopeMagnitude(int32_t baseMagnitude);
    float evaluateReconstructionFilter(ReconFilterState* state, float fallbackValue);
    void pushReconstructionSample(ReconFilterState* state, float newValue);

public:
    void setEnvelope(FFB_SetEnvelope_Data_t* report) override;
    void setPeriodic(FFB_SetPeriodic_Data_t* report) override;
    void updateReconstruction(float new_mag, float new_offset, bool is_periodic);
    
    void setMagnitude(int16_t mag) override { magnitude = mag; }
    int16_t getMagnitude() const override { return magnitude; }
    void setOffset(int16_t off) override { offset = off; }
    int16_t getOffset() const override { return offset; }
    void setPeriod(uint32_t p) override { period = p; }
    uint32_t getPeriod() const override { return period; }
};

class EffectConstant : public EffectTemporal {
public:
    EffectConstant() { type = FFB_EFFECT_CONSTANT; }
    void setConstantForce(FFB_SetConstantForce_Data_t* report) override;
    void setFilters(float calcfrequency, uint8_t profileId) override;
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectRamp : public EffectTemporal {
protected:
    int16_t startLevel = 0;
    int16_t endLevel = 0;
public:
    EffectRamp() { type = FFB_EFFECT_RAMP; }
    void setRamp(FFB_SetRamp_Data_t* report) override;
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectSquare : public EffectTemporal {
public:
    EffectSquare() { type = FFB_EFFECT_SQUARE; }
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectTriangle : public EffectTemporal {
public:
    EffectTriangle() { type = FFB_EFFECT_TRIANGLE; }
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectSawtoothUp : public EffectTemporal {
public:
    EffectSawtoothUp() { type = FFB_EFFECT_SAWTOOTHUP; }
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectSawtoothDown : public EffectTemporal {
public:
    EffectSawtoothDown() { type = FFB_EFFECT_SAWTOOTHDOWN; }
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectSine : public EffectTemporal {
public:
    EffectSine() { type = FFB_EFFECT_SINE; }
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};


// =======================================================================
// BRANCH 2 : EffectConditional (Basés sur la physique de l'axe)
// =======================================================================
class EffectConditional : public Effect {
protected:
    FFB_Effect_Condition conditions[MAX_AXIS];
    int32_t calcConditionEffectForce(float metric, uint8_t idx, float scale);

public:
    EffectConditional() {
        useSingleCondition = false;
    }
    void setCondition(FFB_SetCondition_Data_t* report) override;
    FFB_Effect_Condition* getCondition(uint8_t axis) override {
        if(axis < MAX_AXIS) return &conditions[axis];
        return nullptr;
    }
};

class EffectSpring : public EffectConditional {
public:
    EffectSpring() { type = FFB_EFFECT_SPRING; }
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectDamper : public EffectConditional {
public:
    EffectDamper() { type = FFB_EFFECT_DAMPER; }
    void setFilters(float calcfrequency, uint8_t profileId) override;
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectFriction : public EffectConditional {
public:
    EffectFriction() { type = FFB_EFFECT_FRICTION; }
    void setFilters(float calcfrequency, uint8_t profileId) override;
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

class EffectInertia : public EffectConditional {
public:
    EffectInertia() { type = FFB_EFFECT_INERTIA; }
    void setFilters(float calcfrequency, uint8_t profileId) override;
protected:
    int32_t calculateRawForce(uint8_t axis, metric_t* metrics) override;
};

#endif /* EFFECT_H_ */
