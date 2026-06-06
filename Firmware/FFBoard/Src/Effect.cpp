/*
 * Effect.cpp
 *
 *  Created on: 2026-05-29
 *      Author: Antigravity
 */

#include "Effect.h"
#include "Axis.h"
#include "EffectsCalculator.h" // For globals like reconFilterMode if needed
#include "cppmain.h"

#ifdef USE_DSP_FUNCTIONS
#include "arm_math_types.h"
#include "dsp/fast_math_functions.h"
#include "dsp/interpolation_functions.h"
#define MATH_PI PI
#define MATH_SIN(x) arm_sin_f32(x)
#define MATH_COS(x) arm_cos_f32(x)
#else
#define MATH_PI M_PI
#define MATH_SIN(x) sinf(x)
#define MATH_COS(x) cosf(x)
#endif

#include "EffectsCalculator.h"

// =======================================================================
// Classe de base : Effect
// =======================================================================
int32_t Effect::processForce(uint8_t axis, metric_t* metrics, uint8_t global_gain) {
    if (state == EFFECT_STATE_INACTIVE) return 0;
    
    uint32_t now = HAL_GetTick();
    if (now < startTime) return 0; // Start delay not yet reached

    if (duration != FFB_EFFECT_DURATION_INFINITE && duration != 0) {
        if (now - startTime > duration) {
            state = EFFECT_STATE_INACTIVE; // Effect expired
            return 0; 
        }
    }

    // 1. Calcul spécifique à l'effet
    int32_t raw_force = calculateRawForce(axis, metrics);

    // 2. Application de la composante vectorielle (direction)
    float angle_ratio = axisMagnitudes[axis];
    int32_t final_force = raw_force * angle_ratio;

    // 3. Application des gains locaux (propre à l'effet)
    final_force = (final_force * gain) / 255;

    // 4. Application du filtre spécifique à l'effet
    if (filter[axis] != nullptr && filter[axis]->getFc() < 0.5) {
        final_force = filter[axis]->process(final_force);
    }
    
    // 5. Gain global
    return (final_force * global_gain) / 255;
}

// =======================================================================
// EffectTemporal
// =======================================================================
void EffectTemporal::setEnvelope(FFB_SetEnvelope_Data_t* report) {
    attackLevel = report->attackLevel;
    attackTime = report->attackTime;
    fadeLevel = report->fadeLevel;
    fadeTime = report->fadeTime;
    useEnvelope = true;
}

void EffectPeriodic::setPeriodic(FFB_SetPeriodic_Data_t* report) {
    updateReconstruction((float)report->magnitude, (float)report->offset);
    period = clip<uint32_t,uint32_t>(report->period,1,0x7fff); // Period is never 0
    phase = report->phase;
    magnitude = report->magnitude;
    offset = report->offset;
}

void EffectTemporal::updateReconstruction(float new_mag) {
    reconstructionMagnitude.push(new_mag);
}

void EffectPeriodic::updateReconstruction(float new_mag, float new_offset) {
    EffectTemporal::updateReconstruction(new_mag);
    reconstructionOffset.push(new_offset);
}

int32_t EffectTemporal::getEnvelopeMagnitude(int32_t baseMagnitude) {
    if(duration == FFB_EFFECT_DURATION_INFINITE || duration == 0) {
        return magnitude; // Enveloppe ignorée pour un effet infini
    }
    int32_t scaler = std::abs(baseMagnitude);
    uint32_t elapsed_time = HAL_GetTick() - startTime;
    
    if (elapsed_time < attackTime && attackTime != 0) {
        scaler = (scaler - attackLevel) * elapsed_time;
        scaler /= (int32_t)attackTime;
        scaler += attackLevel;
    }
    if (elapsed_time > (duration - fadeTime) && fadeTime != 0) {
        scaler = (scaler - fadeLevel) * (duration - elapsed_time); // Reversed
        scaler /= (int32_t)fadeTime;
        scaler += fadeLevel;
    }
    return std::signbit(baseMagnitude) ? -scaler : scaler;
}


// =======================================================================
// EffectConstant
// =======================================================================
void EffectConstant::setConstantForce(FFB_SetConstantForce_Data_t* report) {
    magnitude = report->magnitude;
    updateReconstruction((float)magnitude);
}

int32_t EffectConstant::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float interpolated_mag = reconstructionMagnitude.evaluate((float)magnitude, EffectsCalculator::reconFilterMode);
    int32_t current_mag = (int32_t)interpolated_mag; 
    
    if (useEnvelope) {
        return getEnvelopeMagnitude(current_mag);
    }
    return current_mag;
}

// =======================================================================
// EffectRamp
// =======================================================================
void EffectRamp::setRamp(FFB_SetRamp_Data_t* report) {
    magnitude = 0x7fff;
    startLevel = report->startLevel;
    endLevel = report->endLevel;
}

int32_t EffectRamp::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float elapsed_time = (micros()/1000.0) - (float)startTime;
    int32_t dur = duration;
    return (int32_t)startLevel + (elapsed_time * (endLevel - startLevel)) / dur;
}

// =======================================================================
// EffectSquare
// =======================================================================
int32_t EffectSquare::calculateRawForce(uint8_t axis, metric_t* metrics) {
    uint32_t elapsed_time = HAL_GetTick() - startTime;
    float interpolated_mag = reconstructionMagnitude.evaluate((float)magnitude, EffectsCalculator::reconFilterMode);
    float interpolated_offset = reconstructionOffset.evaluate((float)offset, EffectsCalculator::reconFilterMode);
    
    int32_t mag = useEnvelope ? getEnvelopeMagnitude((int32_t)interpolated_mag) : (int32_t)interpolated_mag;
    int32_t force = ((elapsed_time + phase) % ((uint32_t)period + 2)) < (uint32_t)(period + 2) / 2 ? -mag : mag;
    return -(force + (int32_t)interpolated_offset);
}

// =======================================================================
// EffectTriangle
// =======================================================================
int32_t EffectTriangle::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float interpolated_mag = reconstructionMagnitude.evaluate((float)magnitude, EffectsCalculator::reconFilterMode);
    float interpolated_offset = reconstructionOffset.evaluate((float)offset, EffectsCalculator::reconFilterMode);
    int32_t mag = useEnvelope ? getEnvelopeMagnitude((int32_t)interpolated_mag) : (int32_t)interpolated_mag;

    int32_t force = 0;
    float elapsed_time = micros() - ((float)startTime*1000.0);
    float periodF = period;

    int32_t maxMagnitude = interpolated_offset + mag;
    int32_t minMagnitude = interpolated_offset - mag;
    float phasetime = (phase * period) / 35999.0;
    uint32_t timeTemp = elapsed_time + (phasetime*1000); // timetemp in µs
    float remainder = (timeTemp % (period*1000)) / 1000.0f;
    float slope = ((maxMagnitude - minMagnitude) * 2.0f) / periodF;
    if (remainder > (periodF / 2.0f))
        force = slope * (periodF - remainder);
    else
        force = slope * remainder;
    force += minMagnitude;
    return -force;
}

// =======================================================================
// EffectSawtoothUp
// =======================================================================
int32_t EffectSawtoothUp::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float interpolated_mag = reconstructionMagnitude.evaluate((float)magnitude, EffectsCalculator::reconFilterMode);
    float interpolated_offset = reconstructionOffset.evaluate((float)offset, EffectsCalculator::reconFilterMode);
    int32_t mag = useEnvelope ? getEnvelopeMagnitude((int32_t)interpolated_mag) : (int32_t)interpolated_mag;

    float elapsed_time = micros() - ((float)startTime*1000.0);
    float periodF = period;

    float maxMagnitude = interpolated_offset + mag;
    float minMagnitude = interpolated_offset - mag;
    float phasetime = (phase * period) / 35999.0;
    uint32_t timeTemp = elapsed_time + (phasetime*1000); // timetemp in µs
    float remainder = (timeTemp % (period*1000)) / 1000.0f;
    float slope = (maxMagnitude - minMagnitude) / periodF;
    int32_t force_vector = (int32_t)(minMagnitude + slope * (periodF - remainder));
    return -force_vector;
}

// =======================================================================
// EffectSawtoothDown
// =======================================================================
int32_t EffectSawtoothDown::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float interpolated_mag = reconstructionMagnitude.evaluate((float)magnitude, EffectsCalculator::reconFilterMode);
    float interpolated_offset = reconstructionOffset.evaluate((float)offset, EffectsCalculator::reconFilterMode);
    int32_t mag = useEnvelope ? getEnvelopeMagnitude((int32_t)interpolated_mag) : (int32_t)interpolated_mag;

    float elapsed_time = micros() - ((float)startTime*1000.0);
    float periodF = period;

    float maxMagnitude = interpolated_offset + mag;
    float minMagnitude = interpolated_offset - mag;
    float phasetime = (phase * period) / 35999.0;
    uint32_t timeTemp = elapsed_time + (phasetime*1000); // timetemp in µs
    float remainder = (timeTemp % (period*1000)) / 1000.0f;
    float slope = (maxMagnitude - minMagnitude) / periodF;
    int32_t force_vector = (int32_t)(minMagnitude + slope * (remainder)); // reverse time
    return -force_vector;
}

// =======================================================================
// EffectSine
// =======================================================================
int32_t EffectSine::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float t = (micros()/1000.0) - (float)startTime;
    float freq = 1.0f / (float)(std::max<uint32_t>(period, 2));
    float ph = (float)phase / (float)35999;
    
    float interpolated_mag = reconstructionMagnitude.evaluate((float)magnitude, EffectsCalculator::reconFilterMode);
    float interpolated_offset = reconstructionOffset.evaluate((float)offset, EffectsCalculator::reconFilterMode);
    
    int32_t mag = useEnvelope ? getEnvelopeMagnitude((int32_t)interpolated_mag) : (int32_t)interpolated_mag;
    float sine = MATH_SIN(2.0f * MATH_PI * (t * freq + ph)) * mag;
    
    return -((int32_t)interpolated_offset + (int32_t)sine);
}

// =======================================================================
// EffectConditional
// =======================================================================
void EffectConditional::setCondition(FFB_SetCondition_Data_t* report) {
    uint8_t axis = std::min((uint8_t)MAX_AXIS, report->parameterBlockOffset);
    conditions[axis].cpOffset = report->cpOffset;
    conditions[axis].negativeCoefficient = report->negativeCoefficient;
    conditions[axis].positiveCoefficient = report->positiveCoefficient;
    conditions[axis].negativeSaturation = report->negativeSaturation;
    conditions[axis].positiveSaturation = report->positiveSaturation;
    conditions[axis].deadBand = report->deadBand;
    
    if(axis>0 && axis < MAX_AXIS && conditions[axis].isActive()){
        useSingleCondition = false;
    }
    if((conditions[axis].isActive() || (axis > 0 && useSingleCondition)) && axisMagnitudes[axis] == 0){
        axisMagnitudes[axis] = 1.0;
    }
}

int32_t EffectConditional::calcConditionEffectForce(float metric, uint8_t idx, float scale) {
    int16_t off = conditions[idx].cpOffset;
    int16_t deadBand = conditions[idx].deadBand;
    int32_t force = 0;

    if (std::abs(metric - off) > deadBand) {
        float coefficient = (metric > off) ? conditions[idx].positiveCoefficient : conditions[idx].negativeCoefficient;
        coefficient /= 0x7fff; 
        metric = metric - (off + (deadBand * (metric < off ? -1 : 1)));
        
        force = (int32_t)(coefficient * scale * metric);
        
        if(conditions[idx].positiveSaturation != 0 || conditions[idx].negativeSaturation != 0){
             force = clip<int32_t, int32_t>(force, -(int32_t)conditions[idx].negativeSaturation, (int32_t)conditions[idx].positiveSaturation);
        }
    }
    return force;
}

// =======================================================================
// EffectSpring
// =======================================================================
int32_t EffectSpring::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float pos = metrics->pos_scaled_16b;
    uint8_t con_idx = useSingleCondition ? 0 : axis;
    // Note: requires spring gain/scaler
    float spring_scale = 1.0f; // Replace with proper global
    return -calcConditionEffectForce(pos, con_idx, spring_scale);
}

// =======================================================================
// EffectDamper
// =======================================================================
int32_t EffectDamper::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float speed = metrics->speed * INTERNAL_SCALER_DAMPER;
    uint8_t con_idx = useSingleCondition ? 0 : axis;
    return -calcConditionEffectForce(speed, con_idx, 1.0f);
}

// =======================================================================
// EffectFriction
// =======================================================================
int32_t EffectFriction::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float speed = metrics->speed * INTERNAL_SCALER_FRICTION;
    uint8_t con_idx = useSingleCondition ? 0 : axis;
    int16_t off = conditions[con_idx].cpOffset;
    int16_t deadBand = conditions[con_idx].deadBand;
    int32_t force = 0;

    float speedRampupCeil = (EffectsCalculator::frictionPctSpeedToRampup / 100.0) * 32767;

    if (std::abs((int32_t)speed - off) > deadBand) {
        speed -= (off + (deadBand * (speed < off ? -1 : 1)));
        float rampupFactor = 1.0;
        if (std::abs(speed) < speedRampupCeil) {
            float phaseRad = MATH_PI * ((std::abs(speed) / speedRampupCeil) - 0.5f);
            rampupFactor = (1.0f + MATH_SIN(phaseRad)) / 2.0f;
        }

        int8_t sign = speed >= 0 ? 1 : -1;
        uint16_t coeff = speed < 0 ? conditions[con_idx].negativeCoefficient : conditions[con_idx].positiveCoefficient;
        force = coeff * rampupFactor * sign;

        if (conditions[con_idx].negativeSaturation != 0 || conditions[con_idx].positiveSaturation != 0) {
            force = clip<int32_t, int32_t>(force, -conditions[con_idx].negativeSaturation, conditions[con_idx].positiveSaturation);
        }
        return -force; 
    }
    return 0;
}

// =======================================================================
// EffectInertia
// =======================================================================
int32_t EffectInertia::calculateRawForce(uint8_t axis, metric_t* metrics) {
    float accel = metrics->accel * INTERNAL_SCALER_INERTIA;
    uint8_t con_idx = useSingleCondition ? 0 : axis;
    return -calcConditionEffectForce(accel, con_idx, 1.0f);
}
