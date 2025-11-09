/*
 * EffectsCalculator.cpp
 *
 *  Created on: 27.01.21
 *      Author: Jon Lidgard, Yannick Richter, Vincent Manoukian
 */

#include <stdint.h>
#include <math.h>
#include "EffectsCalculator.h"
#include "Axis.h"
#include "ledEffects.h"

#ifdef USE_DSP_FUNCTIONS
#include "arm_math.h"
#endif

#define EFFECT_STATE_INACTIVE 0

ClassIdentifier EffectsCalculator::info = {
		  .name = "Effects" ,
		  .id	= CLSID_EFFECTSCALC,
		  .visibility = ClassVisibility::hidden
};
const ClassIdentifier EffectsCalculator::getInfo(){
	return info;
}

EffectsCalculator::EffectsCalculator() : CommandHandler("fx", CLSID_EFFECTSCALC),
		Thread("FXCalc", EFFECT_THREAD_MEM, EFFECT_THREAD_PRIO)
{
	restoreFlash();

	CommandHandler::registerCommands();
	registerCommand("filterCfFreq", EffectsCalculator_commands::ffbfiltercf, "Constant force filter frequency", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("filterCfQ", EffectsCalculator_commands::ffbfiltercf_q, "Constant force filter Q-factor", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("spring", EffectsCalculator_commands::spring, "Spring gain", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("friction", EffectsCalculator_commands::friction, "Friction gain", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("damper", EffectsCalculator_commands::damper, "Damper gain", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("inertia", EffectsCalculator_commands::inertia, "Inertia gain", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("effects", EffectsCalculator_commands::effects, "USed effects since reset (Info print as str). set 0 to reset", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("effectsDetails", EffectsCalculator_commands::effectsDetails, "List effects details. set 0 to reset", CMDFLAG_GET | CMDFLAG_SET  | CMDFLAG_STR_ONLY | CMDFLAG_GETADR);
	registerCommand("effectsForces", EffectsCalculator_commands::effectsForces, "List actual effects forces.", CMDFLAG_GET | CMDFLAG_GETADR);
//	registerCommand("monitorEffect", EffectsCalculator_commands::monitorEffect, "Get monitoring status. set to 1 to enable.", CMDFLAG_GET | CMDFLAG_SET);

	registerCommand("damper_f", EffectsCalculator_commands::damper_f, "Damper biquad freq", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("damper_q", EffectsCalculator_commands::damper_q, "Damper biquad q", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("friction_f", EffectsCalculator_commands::friction_f, "Friction biquad freq", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("friction_q", EffectsCalculator_commands::friction_q, "Friction biquad q", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("inertia_f", EffectsCalculator_commands::inertia_f, "Inertia biquad freq", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("inertia_q", EffectsCalculator_commands::inertia_q, "Inertia biquad q", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("filterProfile_id", EffectsCalculator_commands::filterProfileId, "Conditional effects filter profile: 0 default; 1 custom", CMDFLAG_GET | CMDFLAG_SET);

	registerCommand("frictionPctSpeedToRampup", EffectsCalculator_commands::frictionPctSpeedToRampup, "% of max speed for gradual increase", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("reconFilterMode", EffectsCalculator_commands::reconFilterMode, "Recon. filter: 0=None, 1=Linear, 2=CubicNatural, 3=CubicHermite", CMDFLAG_GET | CMDFLAG_SET);

	//this->Start(); // Enable if we want to periodically monitor
}

EffectsCalculator::~EffectsCalculator()
{

}


bool EffectsCalculator::isActive()
{
	return effects_active;
}
void EffectsCalculator::setActive(bool active)
{
	effects_active = active;
	for (uint8_t i = 0; i < effects_stats.size(); i++)
	{
		effects_stats[i].current = {0}; // Reset active effect forces
		effects_statslast[i].current = {0};
	}
	setClipLed(active);
}

void EffectsCalculator::updateSamplerate(float newSamplerate){
	this->calcfrequency = newSamplerate;
	for(FFB_Effect &effect : this->effects){
		if(effect.filter[0]){ // Update filters if effect has filters
			setFilters(&effect);
		}
	}
}


/*
If the metric is less than CP Offset - Dead Band, then the resulting force is given by the following formula:
		force = Negative Coefficient * (q - (CP Offset – Dead Band))
Similarly, if the metric is greater than CP Offset + Dead Band, then the resulting force is given by the
following formula:
		force = Positive Coefficient * (q - (CP Offset + Dead Band))
A spring condition uses axis position as the metric.
A damper condition uses axis velocity as the metric.
An inertia condition uses axis acceleration as the metric.

 */

float EffectsCalculator::getReconstructedvalue(ReconFilterState* state, float fallbackValue)
{
    // 'reconFilterMode' est une variable membre de la classe

    float resulting_value = fallbackValue; // Par défaut, valeur "NONE"
    uint32_t now_us = micros();
    float last_known_time = state->spline_x[3]; // Le point le plus récent

    // Timeout de 50ms (si le jeu est en pause, etc.)
    if (!state->isSplineReady || (now_us > last_known_time + 50000)) {
        return state->spline_y[3]; // Maintenir la dernière valeur
    }

    switch(reconFilterMode) {
        case ReconFilterMode::NO_RECONSTRUCTION:
            resulting_value = state->spline_y[3]; // La plus récente (pas de filtre)
            break;

        case ReconFilterMode::LINEAR_INTERPOLATION:
        {
            float t0 = state->spline_x[2]; // Pénultième point
            float t1 = state->spline_x[3]; // Dernier point
            float v0 = state->spline_y[2];
            float v1 = state->spline_y[3];

            float interval_duration = t1 - t0;
            if (interval_duration <= 0) {
                resulting_value = v1;
                break;
            }

            // On interpole *entre* t0 et t1 (pas t1 et "now")
            // Correction : on interpole entre t0 et t1, en projetant "now"
            // MAIS pour la latence la plus faible, on interpole entre t1 et "now",
            // en utilisant l'intervalle t0-t1 comme durée.
            // NON, la logique LERP simple est entre T-1 et T
            float progress = ((float)now_us - t1) / (t1 - t0); // Progrès depuis T
            progress = clip<float>(progress, 0.0f, 1.0f); // Évite l'extrapolation
            resulting_value = v0 + progress * (v1 - v0); // Erreur: v0 +...

            // Correction de la logique Linear:
            // Nous voulons interpoler entre le point [2] et le point [3]
            // "now_us" doit se situer entre spline_x[2] et spline_x[3]

            float progress_lin = ((float)now_us - t0) / interval_duration;
            progress_lin = clip<float>(progress_lin, 0.0f, 1.0f);
            resulting_value = v0 + progress_lin * (v1 - v0);

            // *** RE-CORRECTION ***
            // Votre code original (ligne 201) interpole entre t0 et t1, mais
            // utilise 'now_us - t1_us'. C'est confus.
            // La VRAIE interpolation linéaire (LRF) se fait entre T-1 et T
            // (points 2 et 3) sur la *durée* qui les sépare.
            float t_lin_0 = state->spline_x[2];
            float t_lin_1 = state->spline_x[3];
            float v_lin_0 = state->spline_y[2];
            float v_lin_1 = state->spline_y[3];

            float lin_duration = t_lin_1 - t_lin_0;
            if(lin_duration <= 0) {
                resulting_value = v_lin_1;
                break;
            }
            float lin_progress = ((float)now_us - t_lin_1) / lin_duration;
            // ^^^ ERREUR. "now" doit être entre t0 et t1.
            // La logique correcte est :
            lin_progress = ((float)now_us - t_lin_0) / lin_duration;
            lin_progress = clip<float>(lin_progress, 0.0f, 1.0f);
            resulting_value = v_lin_0 + lin_progress * (v_lin_1 - v_lin_0);

            break;
        }

#ifdef USE_DSP_FUNCTIONS
        // if DSP is enabled we have SPLINE available, else use Hermite
        case ReconFilterMode::SPLINE_CUBIC_NATURAL:
        {
            float interpolated_torque_f = 0.0f;
            float now_f = (float)now_us;

            float interp_time = clip<float>(now_f, state->spline_x[0], state->spline_x[3]);
            arm_spline_f32(&state->spline_instance, &interp_time, &interpolated_torque_f, 1);
            resulting_value = interpolated_torque_f;
            break;
        }
#endif

        case ReconFilterMode::SPLINE_CUBIC_HERMITE:
#ifndef USE_DSP_FUNCTIONS
        // if the DSP is not available, we use Hermite for spline
        case ReconFilterMode::SPLINE_CUBIC_NATURAL:
#endif
        {
            // L'interpolation se fait entre P1 (idx 1) et P2 (idx 2)
            const float p1 = state->spline_y[1];
            const float p2 = state->spline_y[2];
            const float t1 = state->spline_x[1];
            const float t2 = state->spline_x[2];

            float interval = t2 - t1;
            if (interval <= 0) {
                resulting_value = p1;
                break;
            }
            float t = ((float)now_us - t1) / interval;
            t = clip<float>(t, 0.0f, 1.0f); // Pas d'extrapolation

            // Tangentes (Catmull-Rom)
            float m1, m2;
            float dt_m1 = state->spline_x[2] - state->spline_x[0];
            float dt_m2 = state->spline_x[3] - state->spline_x[1];

            if (dt_m1 > 0) m1 = (state->spline_y[2] - state->spline_y[0]) / dt_m1; else m1 = 0;
            if (dt_m2 > 0) m2 = (state->spline_y[3] - state->spline_y[1]) / dt_m2; else m2 = 0;

            m1 *= interval;
            m2 *= interval;

            float tSq = t * t;
            float tCub = tSq * t;
            float h_00 = 2*tCub - 3*tSq + 1;
            float h_10 = tCub - 2*tSq + t;
            float h_01 = -2*tCub + 3*tSq;
            float h_11 = tCub - tSq;

            resulting_value = h_00 * p1 + h_10 * m1 + h_01 * p2 + h_11 * m2;
            break;
        }
    }
    return resulting_value;
}

/**
 * Calculates the resulting torque for FFB effects
 * Takes current position input scaled from -0x7fff to 0x7fff
 * Outputs a torque value from -0x7fff to 0x7fff (not yet clipped)
 */
void EffectsCalculator::calculateEffects(std::vector<std::unique_ptr<Axis>> &axes)
{
	int axisCount = axes.size();
	int64_t forces[MAX_AXIS] = {0};

	if(isActive()){
		int32_t force = 0;
		for (uint8_t i = 0; i < effects_stats.size(); i++)
		{
			effects_stats[i].current = {0}; // Reset active effect forces
		}

		for (uint8_t fxi = 0; fxi < MAX_EFFECTS; fxi++)
		{
			FFB_Effect *effect = &effects[fxi];

			// Effect activated and not infinite (0 or 0xffff)
			if (effect->state != EFFECT_STATE_INACTIVE && effect->duration != FFB_EFFECT_DURATION_INFINITE && effect->duration != 0){
				// Start delay not yet reached
				if(HAL_GetTick() < effect->startTime){
					continue;
				}
				// If effect has expired make inactive
				if (HAL_GetTick() - effect->startTime > effect->duration)
				{
					effect->state = EFFECT_STATE_INACTIVE;
					for(uint8_t axis=0 ; axis < axisCount ; axis++)
						calcStatsEffectType(effect->type, 0,axis); // record a 0 on the ended force
				}
			}

			// Filter out inactive effects
			if (effect->state == EFFECT_STATE_INACTIVE)
			{
				continue;
			}

			force = calcNonConditionEffectForce(effect);	// Compute the effect force

			for(uint8_t axis=0 ; axis < axisCount ; axis++) // Calculate effects for all axes
			{
				int32_t axisforce = calculateEffectForceOnAxis(effect, force, axes, axis);
				calcStatsEffectType(effect->type, axisforce,axis);
				forces[axis] += axisforce; // Do not clip yet to allow effects to subtract force correctly. Will not overflow as maxeffects * 0x7fff is less than int32 range
			}
		}
		effects_statslast = effects_stats;
	}

	// Apply summed force to axes
	for(uint8_t i=0 ; i < axisCount ; i++)
	{
		axes[i]->calculateMechanicalEffects(isActive());
		axes[i]->setFfbEffectTorque(forces[i]);
	}
}

/**
 * Calculates forces from a non conditional effect
 * Periodic and constant effects
 */
int32_t EffectsCalculator::calcNonConditionEffectForce(FFB_Effect *effect) {
	int32_t force_vector = 0;
	
	// Get interpolated magnitude (or amplitude)
	float interpolated_magnitude = getReconstructedvalue(
        &effect->recon_magnitude,    // Utilise la nouvelle structure
        (float)effect->magnitude     // Valeur de fallback (pour le mode NONE)
    );

    // Get interpolated offset
    float interpolated_offset_float = getReconstructedvalue(
        &effect->recon_offset,       // Utilise la nouvelle structure
        (float)effect->offset
    );
    int32_t offset_lrf = (int32_t)interpolated_offset_float;

	// Magnitude with envelope if used
    int32_t magnitude; 
	if(effect->useEnvelope){
        magnitude = getEnvelopeMagnitude(effect, (int32_t)interpolated_magnitude);
	} else {
        magnitude = (int32_t)interpolated_magnitude;
    }


	switch (effect->type){

	case FFB_EFFECT_CONSTANT:
	{ // Constant force is just the force
		force_vector = (int32_t)magnitude;
		break;
	}

	case FFB_EFFECT_RAMP:
	{
		float elapsed_time = (micros()/1000.0) - (float)effect->startTime;
		int32_t duration = effect->duration;
		force_vector = (int32_t)effect->startLevel + (elapsed_time * (effect->endLevel - effect->startLevel)) / duration;
		break;
	}

	case FFB_EFFECT_SQUARE:
	{
		uint32_t elapsed_time = HAL_GetTick() - effect->startTime; // Square is ms aligned
		int32_t force = ((elapsed_time + effect->phase) % ((uint32_t)effect->period + 2)) < (uint32_t)(effect->period + 2) / 2 ? -magnitude : magnitude;
		force_vector = force + offset_lrf;
		break;
	}

	case FFB_EFFECT_TRIANGLE:
	{
		int32_t force = 0;
		int32_t offset = offset_lrf;
		float elapsed_time = micros() - ((float)effect->startTime*1000.0);
		uint32_t phase = effect->phase;
		uint32_t period = effect->period;
		float periodF = period;

		int32_t maxMagnitude = offset + magnitude;
		int32_t minMagnitude = offset - magnitude;
		float phasetime = (phase * period) / 35999.0;
		uint32_t timeTemp = elapsed_time + (phasetime*1000); // timetemp in µs
		float remainder = (timeTemp % (period*1000)) / 1000;
		float slope = ((maxMagnitude - minMagnitude) * 2) / periodF;
		if (remainder > (periodF / 2))
			force = slope * (periodF - remainder);
		else
			force = slope * remainder;
		force += minMagnitude;
		force_vector = force;
		break;
	}

	case FFB_EFFECT_SAWTOOTHUP:
	{
		float offset = offset_lrf;
		float elapsed_time = micros() - ((float)effect->startTime*1000.0);
		uint32_t phase = effect->phase;
		uint32_t period = effect->period;
		float periodF = effect->period;

		float maxMagnitude = offset + magnitude;
		float minMagnitude = offset - magnitude;
		float phasetime = (phase * period) / 35999.0;
		uint32_t timeTemp = elapsed_time + (phasetime*1000); // timetemp in µs
		float remainder = (timeTemp % (period*1000)) / 1000;
		float slope = (maxMagnitude - minMagnitude) / periodF;
		force_vector = (int32_t)(minMagnitude + slope * (period - remainder));
		break;
	}

	case FFB_EFFECT_SAWTOOTHDOWN:
	{
		float offset = offset_lrf;
		float elapsed_time = micros() - ((float)effect->startTime*1000.0);
		float phase = effect->phase;
		uint32_t period = effect->period;
		float periodF = effect->period;

		float maxMagnitude = offset + magnitude;
		float minMagnitude = offset - magnitude;
		float phasetime = (phase * period) / 35999.0;
		uint32_t timeTemp = elapsed_time + (phasetime*1000); // timetemp in µs
		float remainder = (timeTemp % (period*1000)) / 1000;
		float slope = (maxMagnitude - minMagnitude) / periodF;
		force_vector = (int32_t)(minMagnitude + slope * (remainder)); // reverse time
		break;
	}

	case FFB_EFFECT_SINE:
	{
		float t = (micros()/1000.0) - (float)effect->startTime;
		float freq = 1.0f / (float)(std::max<uint16_t>(effect->period, 2));
		float phase = (float)effect->phase / (float)35999; //degrees
#ifdef USE_DSP_FUNCTIONS
		float sine = arm_sin_f32(2.0f * PI * (t * freq + phase)) * magnitude;
#else
		float sine = sinf(2.0f * M_PI * (t * freq + phase)) * magnitude;
#endif
		force_vector = (int32_t)(offset_lrf + sine);
		break;
	}
	default:
		return 0;
		break;
	}

	return (force_vector * effect->gain) / 255;
}



/**
 * @brief Calculates the final force of a single effect on a specific axis.
 * It applies directional scaling and computes conditional effects (spring, damper, etc.) based on the axis's metrics.
 * @param effect The effect to calculate.
 * @param forceVector The base force from a non-conditional calculation (e.g., sine wave value).
 * @param axes The list of all axes.
 * @param axis The index of the axis to calculate the force for.
 * @return The calculated torque for the axis.
 */
int32_t EffectsCalculator::calculateEffectForceOnAxis(FFB_Effect *effect, int32_t forceVector, std::vector<std::unique_ptr<Axis>> &axes, uint8_t axis)
{
	int32_t result_torque = 0;
//	uint16_t direction;
	uint8_t con_idx = effect->useSingleCondition? 0 : axis; // condition block index

	metric_t *metrics = axes[axis]->getMetrics();

	float angle_ratio = effect->axisMagnitudes[axis];

	switch (effect->type)
	{
	case FFB_EFFECT_CONSTANT:
	{
		// Optional filtering to reduce spikes
		if(effect->filter[axis] != nullptr) {
			// if the filter is enabled we apply it
			if (effect->filter[axis]->getFc() < 0.5 && effect->filter[0]->getFc() != 0.0)
			{
				forceVector = effect->filter[axis]->process(forceVector);
			}
		}
	}
	// No break required here. The filter is a special preprocessing case for the constant force effect.
	case FFB_EFFECT_RAMP:
	case FFB_EFFECT_SQUARE:
	case FFB_EFFECT_TRIANGLE:
	case FFB_EFFECT_SAWTOOTHUP:
	case FFB_EFFECT_SAWTOOTHDOWN:
	case FFB_EFFECT_SINE:
	{
		result_torque = -forceVector * angle_ratio;
		break;
	}

	case FFB_EFFECT_SPRING:
	{
		float pos = metrics->pos_scaled_16b;
		result_torque -= calcConditionEffectForce(effect, pos, gain.spring, con_idx, scaler.spring, angle_ratio);
		break;
	}


	/** 	      |	  (rampup is from 0..5% of max velocity)
	 * 			  |	  __________ (after use max coefficient)
	 * 			  |	 /
	 *			  |	/
	 *			  |-
	 * ------------------------  Velocity
	 * 			 -|
	 *			/ |
	 * 		   /  |
	 * 	-------   |
	 * 			  |
	 */
	case FFB_EFFECT_FRICTION: // TODO sometimes unstable.
	{
		float speed = metrics->speed * INTERNAL_SCALER_FRICTION;

		int16_t offset = effect->conditions[con_idx].cpOffset;
		int16_t deadBand = effect->conditions[con_idx].deadBand;
		int32_t force = 0;

		float speedRampupCeil = speedRampupPct();

		// Effect is only active outside deadband + offset
		if (abs((int32_t)speed - offset) > deadBand){

			// remove offset/deadband from metric to compute force
			speed -= (offset + (deadBand * (speed < offset ? -1 : 1)) );

			// check if speed is in the 0..x% to rampup, if is this range, apply a sinusoidale function to smooth the torque (slow near 0, slow around the X% rampup
			float rampupFactor = 1.0;
			if (fabs (speed) < speedRampupCeil) {								// if speed in the range to rampup we apply a sinus curbe to ramup

#ifdef USE_DSP_FUNCTIONS
				float phaseRad = PI * ((fabsf (speed) / speedRampupCeil) - 0.5f);// we start to compute the normalized angle (speed / normalizedSpeed@5%) and translate it of -1/2PI to translate sin on 1/2 periode
				rampupFactor = ( 1.0f + arm_sin_f32(phaseRad ) ) / 2.0f;			// sin value is -1..1 range, we translate it to 0..2 and we scale it by 2
#else
				float phaseRad = M_PI * ((fabsf (speed) / speedRampupCeil) - 0.5f);// we start to compute the normalized angle (speed / normalizedSpeed@5%) and translate it of -1/2PI to translate sin on 1/2 periode
				rampupFactor = ( 1.0f + sinf(phaseRad ) ) / 2.0f;			// sin value is -1..1 range, we translate it to 0..2 and we scale it by 2
#endif
			}

			int8_t sign = speed >= 0 ? 1 : -1;
			uint16_t coeff = speed < 0 ? effect->conditions[con_idx].negativeCoefficient : effect->conditions[con_idx].positiveCoefficient;
			force = coeff * rampupFactor * sign;

			//if there is a saturation, used it to clip result
			if (effect->conditions[con_idx].negativeSaturation !=0 || effect->conditions[con_idx].positiveSaturation !=0) {
				force = clip<int32_t, int32_t>(force, -effect->conditions[con_idx].negativeSaturation, effect->conditions[con_idx].positiveSaturation);
			}

			result_torque -= effect->filter[axis]->process( (((gain.friction + 1) * force) >> 8) * angle_ratio * scaler.friction);
		}

		break;
	}
	case FFB_EFFECT_DAMPER:
	{

		float speed = metrics->speed * INTERNAL_SCALER_DAMPER;
		result_torque -= effect->filter[axis]->process(calcConditionEffectForce(effect, speed, gain.damper, con_idx, scaler.damper, angle_ratio));

		break;
	}

	case FFB_EFFECT_INERTIA:
	{
		float accel = metrics->accel * INTERNAL_SCALER_INERTIA;
		result_torque -= effect->filter[axis]->process(calcConditionEffectForce(effect, accel, gain.inertia, con_idx, scaler.inertia, angle_ratio)); // Bump *60 the inertia feedback

		break;
	}

	default:
		// Unsupported effect
		break;
	}
	return (result_torque * global_gain) / 255; // Apply global gain
}

float EffectsCalculator::speedRampupPct() {
	return (frictionPctSpeedToRampup / 100.0) * 32767;	// compute the normalizedSpeed of pctToRampup factor
}


/**
 * Calculates a conditional effect
 * Takes care of deadband and offsets and scalers
 * Gain of 255 = 1x. Prescale with scale factor
 */
int32_t EffectsCalculator::calcConditionEffectForce(FFB_Effect *effect, float  metric, uint8_t gain,
										 uint8_t idx, float scale, float angle_ratio)
{
	int16_t offset = effect->conditions[idx].cpOffset;
	int16_t deadBand = effect->conditions[idx].deadBand;
	int32_t force = 0;
	float gainfactor = (float)(gain+1) / 256.0;

	// Effect is only active outside deadband + offset
	if (abs(metric - offset) > deadBand){
		float coefficient = effect->conditions[idx].negativeCoefficient;
		if(metric > offset){
			coefficient = effect->conditions[idx].positiveCoefficient;
		}
		coefficient /= 0x7fff; // rescale the coefficient of effect

		// remove offset/deadband from metric to compute force
		metric = metric - (offset + (deadBand * (metric < offset ? -1 : 1)) );

		force = clip<int32_t, int32_t>((coefficient * gainfactor * scale * (float)(metric)),
										-effect->conditions[idx].negativeSaturation,
										 effect->conditions[idx].positiveSaturation);
	}


	return force * angle_ratio;
}

/**
 * Modulates the magnitude of an effect based on time and attack/fade levels
 * During attack time the strength changes from the initial attack level to the normal magnitude which is sustained
 * until the fade time where the strength changes to the fade level until the stop time of the effect.
 * Infinite effects can't have an envelope and return the normal magnitude.
 */
int32_t EffectsCalculator::getEnvelopeMagnitude(FFB_Effect *effect, int32_t baseMagnitude)
{
	if(effect->duration == FFB_EFFECT_DURATION_INFINITE || effect->duration == 0){
		return effect->magnitude; // Effect is infinite. envelope is invalid
	}
	int32_t scaler = abs(baseMagnitude);
	uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
	if (elapsed_time < effect->attackTime && effect->attackTime != 0)
	{
		scaler = (scaler - effect->attackLevel) * elapsed_time;
		scaler /= (int32_t)effect->attackTime;
		scaler += effect->attackLevel;
	}
	if (elapsed_time > (effect->duration - effect->fadeTime) && effect->fadeTime != 0)
	{
		scaler = (scaler - effect->fadeLevel) * (effect->duration - elapsed_time); // Reversed
		scaler /= (int32_t)effect->fadeTime;
		scaler += effect->fadeLevel;
	}
	scaler = signbit(baseMagnitude) ? -scaler : scaler; // Follow original sign of magnitude because envelope has no sign (important for constant force)
	return scaler;
}

void EffectsCalculator::setFilters(FFB_Effect *effect){

	std::function<void(std::unique_ptr<Biquad> &)> fnptr = [=](std::unique_ptr<Biquad> &filter){};

	switch (effect->type)
	{
	case FFB_EFFECT_DAMPER:
		fnptr = [=, this](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, this->filter[filterProfileId].damper.freq/ (float)calcfrequency, this->filter[filterProfileId].damper.q * qfloatScaler , (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, this->filter[filterProfileId].damper.freq / (float)calcfrequency, this->filter[filterProfileId].damper.q * qfloatScaler, (float)0.0);
		};
		break;
	case FFB_EFFECT_FRICTION:
		fnptr = [=, this](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, this->filter[filterProfileId].friction.freq / (float)calcfrequency, this->filter[filterProfileId].friction.q * qfloatScaler, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, this->filter[filterProfileId].friction.freq / (float)calcfrequency, this->filter[filterProfileId].friction.q * qfloatScaler, (float)0.0);
		};
		break;
	case FFB_EFFECT_INERTIA:
		fnptr = [=, this](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, this->filter[filterProfileId].inertia.freq / (float)calcfrequency, this->filter[filterProfileId].inertia.q * qfloatScaler, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, this->filter[filterProfileId].inertia.freq / (float)calcfrequency, this->filter[filterProfileId].inertia.q * qfloatScaler, (float)0.0);
		};
		break;
	case FFB_EFFECT_CONSTANT:
		fnptr = [=, this](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, this->filter[0].constant.freq / (float)calcfrequency, this->filter[0].constant.q * qfloatScaler, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, this->filter[0].constant.freq / (float)calcfrequency, this->filter[0].constant.q * qfloatScaler, (float)0.0);
		};
		break;
	}


	for (int i=0; i<MAX_AXIS; i++) {
		fnptr(effect->filter[i]);
	}
}

void EffectsCalculator::pushReconParam(ReconFilterState* state, float newValue)
{
    uint32_t now_us = micros();

    // Shift the spline points
    for (int i = 0; i < 3; ++i) {
        state->spline_x[i] = state->spline_x[i + 1];
        state->spline_y[i] = state->spline_y[i + 1];
    }

    // Add the new point
    state->spline_x[3] = (float)now_us;
    state->spline_y[3] = newValue;

    // FIll in initial points if not ready yet at 60Hz, so 16.66ms intervals
    if (!state->isSplineReady) {
        float fake_time_step = 16666.0f; // 60Hz
        state->spline_x[0] = state->spline_x[3] - 3.0f * fake_time_step;
        state->spline_y[0] = state->spline_y[3];
        state->spline_x[1] = state->spline_x[3] - 2.0f * fake_time_step;
        state->spline_y[1] = state->spline_y[3];
        state->spline_x[2] = state->spline_x[3] - 1.0f * fake_time_step;
        state->spline_y[2] = state->spline_y[3];
        state->isSplineReady = true; // Ready after first fill
    }

    // Initialize spline if needed
    if (reconFilterMode == ReconFilterMode::SPLINE_CUBIC_NATURAL) {
#ifdef USE_DSP_FUNCTIONS
        arm_spline_init_f32(
            &state->spline_instance,
            ARM_SPLINE_NATURAL,
            state->spline_x,
            state->spline_y,
            4,
            state->spline_y2,
            state->spline_scratch
        );
#endif
    }
}

void EffectsCalculator::pushReconFilterValue(FFB_Effect* effect, float new_magnitude, float new_offset, bool is_periodic)
{
	// Update target variables (for NONE mode and fallback)
    effect->magnitude = (int16_t)new_magnitude;

    // Push new magnitude into recon structure
    pushReconParam(&effect->recon_magnitude, new_magnitude);

    // Push new offset into recon structure if periodic
    if (is_periodic) {
        effect->offset = (int16_t)new_offset;
        pushReconParam(&effect->recon_offset, new_offset);
    }
}


void EffectsCalculator::setGain(uint8_t gain)
{
	global_gain = gain;
}

uint8_t EffectsCalculator::getGain() { return global_gain; }

/*
 * Read parameters from flash and restore settings
 */
void EffectsCalculator::restoreFlash()
{
	uint16_t filterStorage;
	if (Flash_Read(ADR_FFB_CF_FILTER, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		checkFilterCoeff(&(this->filter[0].constant), freq, q);
		updateFilterSettingsForEffects(FFB_EFFECT_CONSTANT);
	}

	if (Flash_Read(ADR_FFB_FR_FILTER, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		checkFilterCoeff(&(this->filter[CUSTOM_PROFILE_ID].friction), freq, q);
		updateFilterSettingsForEffects(FFB_EFFECT_FRICTION);
	}

	if (Flash_Read(ADR_FFB_DA_FILTER, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		checkFilterCoeff(&(this->filter[CUSTOM_PROFILE_ID].damper), freq, q);
		updateFilterSettingsForEffects(FFB_EFFECT_DAMPER);
	}

	if (Flash_Read(ADR_FFB_IN_FILTER, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		checkFilterCoeff(&(this->filter[CUSTOM_PROFILE_ID].inertia), freq, q);
		updateFilterSettingsForEffects(FFB_EFFECT_INERTIA);
	}

	uint16_t effects = 0;
	if(Flash_Read(ADR_FFB_EFFECTS1, &effects)){
		gain.friction = (effects >> 8) & 0xff;
		gain.inertia = (effects & 0xff);
	}
	if(Flash_Read(ADR_FFB_EFFECTS2, &effects)){
		gain.damper = (effects >> 8) & 0xff;
		gain.spring = (effects & 0xff);
	}
	if(Flash_Read(ADR_FFB_EFFECTS3, &effects)){
		filterProfileId = (effects >> 8) & 0x03;
		frictionPctSpeedToRampup = (effects & 0xff);
	}

	// Read reconstruction parameters
	if(Flash_Read(ADR_FFB_RECONSTRUCTION_FILTER, &reconFilterMode)){
		reconFilterMode = effects & 0x03;
	}

}

// Saves parameters to flash
void EffectsCalculator::saveFlash()
{
	uint16_t filterStorage;

	// save CF biquad
	filterStorage = (uint16_t)filter[0].constant.freq & 0x1FF;
	filterStorage |= ( (uint16_t)filter[0].constant.q & 0x7F ) << 9 ;
	Flash_Write(ADR_FFB_CF_FILTER, filterStorage);

	if(filterProfileId == CUSTOM_PROFILE_ID){ // Only attempt saving if custom profile active
		// save Friction biquad
		filterStorage = (uint16_t)filter[CUSTOM_PROFILE_ID].friction.freq & 0x1FF;
		filterStorage |= ( (uint16_t)filter[CUSTOM_PROFILE_ID].friction.q & 0x7F ) << 9 ;
		Flash_Write(ADR_FFB_FR_FILTER, filterStorage);

		// save Damper biquad
		filterStorage = (uint16_t)filter[CUSTOM_PROFILE_ID].damper.freq & 0x1FF;
		filterStorage |= ( (uint16_t)filter[CUSTOM_PROFILE_ID].damper.q & 0x7F ) << 9 ;
		Flash_Write(ADR_FFB_DA_FILTER, filterStorage);

		// save Inertia biquad
		filterStorage = (uint16_t)filter[CUSTOM_PROFILE_ID].inertia.freq & 0x1FF;
		filterStorage |= ( (uint16_t)filter[CUSTOM_PROFILE_ID].inertia.q & 0x7F ) << 9 ;
		Flash_Write(ADR_FFB_IN_FILTER, filterStorage);
	}

	// save the effect gain
	uint16_t effects = gain.inertia | (gain.friction << 8);
	Flash_Write(ADR_FFB_EFFECTS1, effects);

	effects = gain.spring | (gain.damper << 8);
	Flash_Write(ADR_FFB_EFFECTS2, effects);

	// save the friction rampup zone
	effects = frictionPctSpeedToRampup | (filterProfileId << 8);
	Flash_Write(ADR_FFB_EFFECTS3, effects);

	// Save reconstruction parameters
	Flash_Write(ADR_FFB_RECONSTRUCTION_FILTER, reconFilterMode);
}

void EffectsCalculator::checkFilterCoeff(biquad_constant_t *filter, uint32_t freq,uint8_t q)
{
	if(q == 0) {
		q = 1;
	}

	if(freq == 0){
		freq = calcfrequency / 2;
	}

	filter->freq = clip<uint32_t, uint32_t>(freq, 1, (calcfrequency / 2));
	filter->q = clip<uint8_t, uint8_t>(q,0,127);
}

void EffectsCalculator::updateFilterSettingsForEffects(uint8_t type_effect) {

	// loop on all effect in memory and setup new constant filter
	for (uint8_t i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i].type == type_effect)
		{
			setFilters(&effects[i]);
		}
	}
}


void EffectsCalculator::logEffectType(uint8_t type,bool remove){
	if(type > 0 && type < 32){

		if(remove){
			if(effects_stats[type-1].nb > 0)
				effects_stats[type-1].nb--;

			if(!effects_stats[type-1].nb){
				//effects_used &= ~(1<<(type-1)); // Only manual reset
				//effects_stats[type-1].max = 0;
				effects_stats[type-1].current = {0};
			}
		}else{
			effects_used |= 1<<(type-1);
			if( effects_stats[type-1].nb < 65535 ) {
				effects_stats[type-1].nb ++;
			}
		}
	}
}

void EffectsCalculator::logEffectState(uint8_t type,uint8_t state){
	if(type > 0 && type < 32){
		if(!state){
			// effects_stats[type-1].max = 0;
			effects_stats[type-1].current = {0};
		}
	}
}


void EffectsCalculator::calcStatsEffectType(uint8_t type, int32_t force,uint8_t axis){
	if(axis >= MAX_AXIS)
		return;
	if(type > 0 && type < 13) {
		uint8_t arrayLocation = type - 1;
		effects_stats[arrayLocation].current[axis] = clip<int32_t,int32_t>(effects_stats[arrayLocation].current[axis] + force, -0x7fff, 0x7fff);
		effects_stats[arrayLocation].max[axis] = std::max(effects_stats[arrayLocation].max[axis], (int16_t)abs(force));
	}
}

/**
 * Prints a list of effects that were active at some point
 * Does not reset when an effect is deactivated
 * Axis only used in detail mode
 */
std::string EffectsCalculator::listEffectsUsed(bool details,uint8_t axis){
	std::string effects_list = "";
	if(axis >= MAX_AXIS)
		return "";

	if (!details) {
		if(effects_used == 0){
			return "None";
		}

		static const char *effects[12] = {"Constant,","Ramp,","Square,","Sine,","Triangle,","Sawtooth Up,","Sawtooth Down,","Spring,","Damper,","Inertia,","Friction,","Custom,"};

		for (int i=0;i < 12; i++) {
			if((effects_used >> i) & 1) {
				effects_list += effects[i];
			}
		}

		effects_list.pop_back();
	} else {

		bool firstItem = true;
		for (int i=0;i < 12; i++) {
			if (!firstItem) effects_list += ", ";
			effects_list += "{\"max\":" + std::to_string(effects_stats[i].max[axis]);
			effects_list += ", \"curr\":" + std::to_string(effects_stats[i].current[axis]);
			effects_list += ", \"nb\":" + std::to_string(effects_stats[i].nb) + "}";
			firstItem = false;
		}

	}

	return effects_list.c_str();
}

/**
 * Resets the effects_used flags
 * If reinit is true it will set the flag again if the currently active effect number is not 0
 */
void EffectsCalculator::resetLoggedActiveEffects(bool reinit){
	effects_used = 0;
	if(reinit){
		for (int i=0;i < 12; i++) {
			if(effects_stats[i].nb > 0) {
				effects_used |= 1<<(i);
			}
		}
	}
}

CommandStatus EffectsCalculator::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<EffectsCalculator_commands>(cmd.cmdId)){

	case EffectsCalculator_commands::ffbfiltercf:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter[0].constant.freq);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter[0].constant, cmd.val, filter[0].constant.q);
			updateFilterSettingsForEffects(FFB_EFFECT_CONSTANT);
		}
		break;
	case EffectsCalculator_commands::ffbfiltercf_q:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->qfloatScaler));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter[0].constant.q);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter[0].constant, filter[0].constant.freq, cmd.val);
			updateFilterSettingsForEffects(FFB_EFFECT_CONSTANT);
		}
		break;
	case EffectsCalculator_commands::effects:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(effects_used); //listEffectsUsed(cmd.val)
		}
		else if (cmd.type == CMDtype::set)
		{
			resetLoggedActiveEffects(cmd.val == 0);
		}
		else if (cmd.type == CMDtype::info)
		{
			replies.emplace_back(listEffectsUsed(false));
		}
		break;
	case EffectsCalculator_commands::effectsDetails:
		if (cmd.type == CMDtype::get || cmd.type == CMDtype::getat)
		{
			replies.emplace_back(listEffectsUsed(true,cmd.adr));
		}
		else if (cmd.type == CMDtype::set && cmd.val >= 0)
		{
			for (int i=0; i<12; i++) {
				effects_stats[i].max = {0};
				if(cmd.val > 0){
					effects_stats[i].current = {0};
					effects_stats[i].nb = 0;
				}
			}
			resetLoggedActiveEffects(true);
		}
		break;
	case EffectsCalculator_commands::effectsForces:
	{
		uint8_t axis = 0;
		if(cmd.type == CMDtype::getat){
			axis = std::min<uint8_t>(cmd.adr,MAX_AXIS);
		}
		if (cmd.type == CMDtype::get || cmd.type == CMDtype::getat)
		{
			for (size_t i=0; i < effects_statslast.size(); i++) {
				replies.emplace_back(effects_statslast[i].current[axis],effects_statslast[i].nb);
			}
		}
		break;
	}
	case EffectsCalculator_commands::spring:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->scaler.spring));
		}else
			return handleGetSet(cmd, replies, this->gain.spring);
		break;
	case EffectsCalculator_commands::friction:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->scaler.friction)+",factor:"+std::to_string(INTERNAL_SCALER_FRICTION));
		}else
			return handleGetSet(cmd, replies, this->gain.friction);
		break;
	case EffectsCalculator_commands::damper:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->scaler.damper)+",factor:"+std::to_string(INTERNAL_SCALER_DAMPER));
		}else
			return handleGetSet(cmd, replies, this->gain.damper);
		break;
	case EffectsCalculator_commands::inertia:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->scaler.inertia)+",factor:"+std::to_string(INTERNAL_SCALER_INERTIA));
		}else
			return handleGetSet(cmd, replies, this->gain.inertia);
		break;
	case EffectsCalculator_commands::damper_f:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter[filterProfileId].damper.freq);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter[CUSTOM_PROFILE_ID].damper, cmd.val, filter[CUSTOM_PROFILE_ID].damper.q);
			if (filterProfileId == CUSTOM_PROFILE_ID) updateFilterSettingsForEffects(FFB_EFFECT_DAMPER);
		}
		break;
	case EffectsCalculator_commands::damper_q:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter[filterProfileId].damper.q);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter[CUSTOM_PROFILE_ID].damper, filter[CUSTOM_PROFILE_ID].damper.freq, cmd.val);
			if (filterProfileId == CUSTOM_PROFILE_ID) updateFilterSettingsForEffects(FFB_EFFECT_DAMPER);
		}
		break;
	case EffectsCalculator_commands::friction_f:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter[filterProfileId].friction.freq);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter[CUSTOM_PROFILE_ID].friction, cmd.val, filter[CUSTOM_PROFILE_ID].friction.q);
			if (filterProfileId == CUSTOM_PROFILE_ID) updateFilterSettingsForEffects(FFB_EFFECT_FRICTION);
		}
		break;
	case EffectsCalculator_commands::friction_q:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter[filterProfileId].friction.q);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter[CUSTOM_PROFILE_ID].friction, filter[CUSTOM_PROFILE_ID].friction.freq, cmd.val);
			if (filterProfileId == CUSTOM_PROFILE_ID) updateFilterSettingsForEffects(FFB_EFFECT_FRICTION);
		}
		break;
	case EffectsCalculator_commands::inertia_f:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter[filterProfileId].inertia.freq);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter[CUSTOM_PROFILE_ID].inertia, cmd.val, filter[CUSTOM_PROFILE_ID].inertia.q);
			if (filterProfileId == CUSTOM_PROFILE_ID) updateFilterSettingsForEffects(FFB_EFFECT_INERTIA);
		}
		break;
	case EffectsCalculator_commands::inertia_q:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter[filterProfileId].inertia.q);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter[CUSTOM_PROFILE_ID].inertia, filter[CUSTOM_PROFILE_ID].inertia.freq, cmd.val);
			if (filterProfileId == CUSTOM_PROFILE_ID) updateFilterSettingsForEffects(FFB_EFFECT_INERTIA);
		}
		break;

	case EffectsCalculator_commands::frictionPctSpeedToRampup:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(frictionPctSpeedToRampup);
		}
		else if (cmd.type == CMDtype::set)
		{
			uint8_t pct = clip<uint8_t, uint8_t>(cmd.val, 0, 100);
			frictionPctSpeedToRampup = pct;
		}
		break;
	case EffectsCalculator_commands::filterProfileId:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(this->filterProfileId);
		}
		else if (cmd.type == CMDtype::set)
		{
			uint32_t value = clip<uint32_t, uint32_t>(cmd.val, 0, 1);
			this->filterProfileId = value;
			updateFilterSettingsForEffects(FFB_EFFECT_INERTIA);
			updateFilterSettingsForEffects(FFB_EFFECT_DAMPER);
			updateFilterSettingsForEffects(FFB_EFFECT_FRICTION);
		}
		break;
	case EffectsCalculator_commands::monitorEffect:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(isMonitorEffect);
		}
		else if (cmd.type == CMDtype::set)
		{
			isMonitorEffect = clip<uint8_t, uint8_t>(cmd.val, 0, 1);
		}
		break;

	case EffectsCalculator_commands::reconFilterMode:
        if (cmd.type == CMDtype::get) {
            replies.emplace_back((uint32_t)reconFilterMode);
        } else if (cmd.type == CMDtype::set) {
            uint32_t mode = clip<uint32_t, uint32_t>(cmd.val, 0, 3);
            reconFilterMode = (ReconFilterMode)mode;
        }
        break;

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}

/*
 *
 */
void EffectsCalculator::Run() {
	std::vector<CommandReply> replies;
	Delay(500);
	while (true) {
		Delay(3000);

		if(isMonitorEffect) {

			continue; // TODO uncomment when stream is ok
			replies.push_back(CommandReply(listEffectsUsed(true)));
			CommandInterface::broadcastCommandReplyAsync(replies,
					this,
					(uint32_t)EffectsCalculator_commands::effectsDetails,
					CMDtype::get);

		}

	}

}

/**
 * Resets an effect and marks the effect as free
 */
void EffectsCalculator::free_effect(uint16_t idx){
	if(idx < this->effects.size()){
		logEffectType(effects[idx].type, true); // Effect off
		effects[idx] = FFB_Effect(); // Reset all settings
		for(int i=0; i< MAX_AXIS; i++) {
			if(effects[idx].filter[i] != nullptr){
				effects[idx].filter[i].reset(nullptr);
			}
		}
	}
}

/**
 * Will return the first effect index which is empty or -1 if none found
 */
int32_t EffectsCalculator::find_free_effect(uint8_t type){
	if(type > FFB_EFFECT_NONE && type < FFB_EFFECT_CUSTOM+1){ // Check if it is a valid effect type
		for(uint8_t i=0;i<effects.size();i++){
			if(effects[i].type == FFB_EFFECT_NONE){
				return(i);
			}
		}
	}
	return -1;
}




/**
 * Calculates the frequency of hid out reports
 */
uint32_t EffectsControlItf::getRate(){
	float periodAvg = fxPeriodAvg.getAverage();
	if((micros() - lastFxUpdate) > 1000000 || periodAvg == 0){
		// Reset average
		fxPeriodAvg.clear();
		return 0;
	}else{
		return (1000000.0/periodAvg);
	}
}

/**
 * Calculates the frequency of the CF effect only
 */
uint32_t EffectsControlItf::getConstantForceRate(){
	float periodAvg = cfUpdatePeriodAvg.getAverage();
	if((micros() - lastCfUpdate) > 1000000 || periodAvg == 0){
		// Reset average
		cfUpdatePeriodAvg.clear();
		return 0;
	}else{
		return (1000000.0/periodAvg);
	}
}


void EffectsControlItf::cfUpdateEvent(){
	cfUpdatePeriodAvg.addValue((uint32_t)(micros() - lastCfUpdate));
	lastCfUpdate = micros();
}

void EffectsControlItf::fxUpdateEvent(){
	fxPeriodAvg.addValue((uint32_t)(micros() - lastFxUpdate));
	lastFxUpdate = micros();
}
