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

#define X_AXIS_ENABLE 1
#define Y_AXIS_ENABLE 2
#define Z_AXIS_ENABLE 4
#define DIRECTION_ENABLE(AXES) (1 << AXES)

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
		Thread("EffectsCalculator", EFFECT_THREAD_MEM, EFFECT_THREAD_PRIO)
{
	restoreFlash();

	CommandHandler::registerCommands();
	registerCommand("filterCfFreq", EffectsCalculator_commands::ffbfiltercf, "Constant force filter frequency", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("filterCfQ", EffectsCalculator_commands::ffbfiltercf_q, "Constant force filter Q-factor", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("spring", EffectsCalculator_commands::spring, "Spring gain", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("friction", EffectsCalculator_commands::friction, "Friction gain", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("damper", EffectsCalculator_commands::damper, "Damper gain", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("inertia", EffectsCalculator_commands::inertia, "Inertia gain", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("effects", EffectsCalculator_commands::effects, "List effects. set 0 to reset", CMDFLAG_GET | CMDFLAG_SET  | CMDFLAG_STR_ONLY);
	registerCommand("effectsDetails", EffectsCalculator_commands::effectsDetails, "List effects details. set 0 to reset", CMDFLAG_GET | CMDFLAG_SET  | CMDFLAG_STR_ONLY);
	registerCommand("effectsForces", EffectsCalculator_commands::effectsForces, "List actual effects forces.", CMDFLAG_GET | CMDFLAG_STR_ONLY);
	registerCommand("monitorEffect", EffectsCalculator_commands::monitorEffect, "Get monitoring status. set to 1 to enable.", CMDFLAG_GET | CMDFLAG_SET);

	registerCommand("damper_f", EffectsCalculator_commands::damper_f, "Damper biquad freq", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("damper_q", EffectsCalculator_commands::damper_q, "Damper biquad q", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("friction_f", EffectsCalculator_commands::friction_f, "Friction biquad freq", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("friction_q", EffectsCalculator_commands::friction_q, "Friction biquad q", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("inertia_f", EffectsCalculator_commands::inertia_f, "Inertia biquad freq", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("inertia_q", EffectsCalculator_commands::inertia_q, "Inertia biquad q", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);

	registerCommand("frictionPctSpeedToRampup", EffectsCalculator_commands::frictionPctSpeedToRampup, "% of max speed during effect is slow", CMDFLAG_GET | CMDFLAG_SET);

	this->Start();
}

EffectsCalculator::~EffectsCalculator()
{
	this->Suspend();
}


bool EffectsCalculator::isActive()
{
	return effects_active;
}
void EffectsCalculator::setActive(bool active)
{
	effects_active = active;
}

/**
 * Sets the mask where the direction enable bit is in the effect
 */
void EffectsCalculator::setDirectionEnableMask(uint8_t mask){
	this->directionEnableMask = mask;
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

If the number of Condition report blocks is equal to the number of axes for the effect, then the first report
block applies to the first axis, the second applies to the second axis, and so on. For example, a two-axis
spring condition with CP Offset set to zero in both Condition report blocks would have the same effect as
the joystick self-centering spring. When a condition is defined for each axis in this way, the effect must
not be rotated.

If there is a single Condition report block for an effect with more than one axis, then the direction along
which the parameters of the Condition report block are in effect is determined by the direction parameters
passed in the Direction field of the Effect report block. For example, a friction condition rotated 45
degrees (in polar coordinates) would resist joystick motion in the northeast-southwest direction but would
have no effect on joystick motion in the northwest-southeast direction.
 */

/*
 * Calculates the resulting torque for FFB effects
 * Takes current position input scaled from -0x7fff to 0x7fff
 * Outputs a torque value from -0x7fff to 0x7fff (not yet clipped)
 */
void EffectsCalculator::calculateEffects(std::vector<std::unique_ptr<Axis>> &axes)
{
	for (auto &axis : axes) {
		axis->calculateAxisEffects(isActive());
	}

	if(!isActive()){
	 return;
	}

	int32_t forceX = 0;
	int32_t forceY = 0;
	int32_t forceVector = 0;
	uint8_t axisCount = axes.size();
	bool validY = axisCount > 1;
#if MAX_AXIS == 3
	int32_t forceZ = 0;
	bool validZ = axisCount > 2;
#endif

	for (uint8_t i = 0; i < MAX_EFFECTS; i++)
	{
		FFB_Effect *effect = &effects[i];

		// Effect activated and not infinite (0 or 0xffff)
		if (effect->state != EFFECT_STATE_INACTIVE && effect->duration != FFB_EFFECT_DURATION_INFINITE && effect->duration != 0){
			// Start delay not yet reached
			if(HAL_GetTick() < effect->startTime){
				continue;
			}
			// If effect has expired make inactive
			if (HAL_GetTick() > effect->startTime + effect->duration)
			{
				effect->state = EFFECT_STATE_INACTIVE;
				calcStatsEffectType(effect->type, 0); // record a 0 on the ended force
			}
		}

		// Filter out inactive effects
		if (effect->state == EFFECT_STATE_INACTIVE)
		{
			continue;
		}


		//if (effect->conditionsCount == 0) {
		forceVector = calcNonConditionEffectForce(effect);
		//}

		uint8_t directionEnableMask = this->directionEnableMask ? this->directionEnableMask : DIRECTION_ENABLE(axisCount);

		if (effect->enableAxis & directionEnableMask || (effect->enableAxis & X_AXIS_ENABLE))
		{
			int32_t newEffectForce = calcComponentForce(effect, forceVector, axes, 0);
			calcStatsEffectType(effect->type, newEffectForce);
			forceX += newEffectForce;
			forceX = clip<int32_t, int32_t>(forceX, -0x7fff, 0x7fff); // Clip
		}
		if (validY && (effect->enableAxis & directionEnableMask || (effect->enableAxis & Y_AXIS_ENABLE)))
		{
			int32_t newEffectForce = calcComponentForce(effect, forceVector, axes, 0);
			calcStatsEffectType(effect->type, newEffectForce);
			forceY += newEffectForce;
			forceY = clip<int32_t, int32_t>(forceY, -0x7fff, 0x7fff); // Clip
		}

	}

	axes[0]->setEffectTorque(forceX);
	if (validY)
	{
		axes[1]->setEffectTorque(forceY);
	}
}

/**
 * Calculates forces from a non conditional effect
 * Periodic and constant effects
 */
int32_t EffectsCalculator::calcNonConditionEffectForce(FFB_Effect *effect) {
	int32_t force_vector = 0;
	int32_t magnitude = effect->magnitude;

	// If using an envelope modulate the magnitude based on time
	if(effect->useEnvelope){
		magnitude = getEnvelopeMagnitude(effect);
	}
	switch (effect->type){

	case FFB_EFFECT_CONSTANT:
	{ // Constant force is just the force
		force_vector = (int32_t)magnitude;
		break;
	}

	case FFB_EFFECT_RAMP:
	{
		uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
		int32_t duration = effect->duration;
		force_vector = (int32_t)effect->startLevel + ((int32_t)elapsed_time * (effect->endLevel - effect->startLevel)) / duration;
		break;
	}

	case FFB_EFFECT_SQUARE:
	{
		uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
		int32_t force = ((elapsed_time + effect->phase) % ((uint32_t)effect->period + 2)) < (uint32_t)(effect->period + 2) / 2 ? -magnitude : magnitude;
		force_vector = force + effect->offset;
		break;
	}

	case FFB_EFFECT_TRIANGLE:
	{
		int32_t force = 0;
		int32_t offset = effect->offset;
		uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
		uint32_t phase = effect->phase;
		uint32_t period = effect->period;
		float periodF = period;

		int32_t maxMagnitude = offset + magnitude;
		int32_t minMagnitude = offset - magnitude;
		uint32_t phasetime = (phase * period) / 35999;
		uint32_t timeTemp = elapsed_time + phasetime;
		float remainder = timeTemp % period;
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
		float offset = effect->offset;
		uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
		uint32_t phase = effect->phase;
		uint32_t period = effect->period;
		float periodF = effect->period;

		float maxMagnitude = offset + magnitude;
		float minMagnitude = offset - magnitude;
		int32_t phasetime = (phase * period) / 35999;
		uint32_t timeTemp = elapsed_time + phasetime;
		float remainder = timeTemp % period;
		float slope = (maxMagnitude - minMagnitude) / periodF;
		force_vector = (int32_t)(minMagnitude + slope * (period - remainder));
		break;
	}

	case FFB_EFFECT_SAWTOOTHDOWN:
	{
		float offset = effect->offset;
		uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
		float phase = effect->phase;
		uint32_t period = effect->period;
		float periodF = effect->period;

		float maxMagnitude = offset + magnitude;
		float minMagnitude = offset - magnitude;
		int32_t phasetime = (phase * period) / 35999;
		uint32_t timeTemp = elapsed_time + phasetime;
		float remainder = timeTemp % period;
		float slope = (maxMagnitude - minMagnitude) / periodF;
		force_vector = (int32_t)(minMagnitude + slope * (remainder)); // reverse time
		break;
	}

	case FFB_EFFECT_SINE:
	{
		float t = HAL_GetTick() - effect->startTime;
		float freq = 1.0f / (float)(std::max<uint16_t>(effect->period, 2));
		float phase = (float)effect->phase / (float)35999; //degrees
		float sine = sinf(2.0 * M_PI * (t * freq + phase)) * magnitude;
		force_vector = (int32_t)(effect->offset + sine);
		break;
	}
	default:
		return 0;
		break;
	}

	return (force_vector * effect->gain) / 255;
}



/*
 * If the number of Condition report blocks is equal to the number of axes for the effect, then the first report
block applies to the first axis, the second applies to the second axis, and so on. For example, a two-axis
spring condition with CP Offset set to zero in both Condition report blocks would have the same effect as
the joystick self-centering spring. When a condition is defined for each axis in this way, the effect must
not be rotated.

If there is a single Condition report block for an effect with more than one axis, then the direction along
which the parameters of the Condition report block are in effect is determined by the direction parameters
passed in the Direction field of the Effect report block. For example, a friction condition rotated 45
degrees (in polar coordinates) would resist joystick motion in the northeast-southwest direction but would
have no effect on joystick motion in the northwest-southeast direction.
 */

int32_t EffectsCalculator::calcComponentForce(FFB_Effect *effect, int32_t forceVector, std::vector<std::unique_ptr<Axis>> &axes, uint8_t axis)
{
	int32_t result_torque = 0;
	uint16_t direction;
	uint8_t con_idx = 0; // condition block index

	metric_t *metrics = axes[axis]->getMetrics();
	uint8_t axisCount = axes.size();
	uint8_t directionEnableMask = this->directionEnableMask ? this->directionEnableMask : DIRECTION_ENABLE(axisCount);
	if (effect->enableAxis & directionEnableMask)
	{
		direction = effect->directionX;
		con_idx = axis;
	}
	else
	{
		direction = axis == 0 ? effect->directionX : effect->directionY;
		con_idx = axis;
	}

	//bool useForceDirectionForConditionEffect = (effect->enableAxis == DIRECTION_ENABLE && axisCount > 1 && effect->conditionsCount == 1);
	bool rotateConditionForce = (axisCount > 1); // && effect->conditionsCount < axisCount
	float angle = ((float)direction * (2*M_PI) / 36000.0);
	float angle_ratio = axis == 0 ? sin(angle) : -1 * cos(angle);
	angle_ratio = rotateConditionForce ? angle_ratio : 1.0;

	switch (effect->type)
	{
	case FFB_EFFECT_CONSTANT:
	{
		// Optional filtering to reduce spikes
		if(effect->filter[con_idx] != nullptr) {
			// if the filter is enabled we apply it
			if (effect->filter[con_idx]->getFc() < 0.5 && effect->filter[0]->getFc() != 0.0)
			{
				result_torque = effect->filter[con_idx]->process(forceVector);
			}
		}
	}
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
		float pos = metrics->pos;
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

		// Effect is only active outside deadband + offset
		if (abs((int32_t)speed - offset) > deadBand){

			// remove offset/deadband from metric to compute force
			speed -= (offset + (deadBand * (speed < offset ? -1 : 1)) );

			// check if speed is in the 0..x% to rampup, if is this range, apply a sinusoidale function to smooth the torque (slow near 0, slow around the X% rampup
			float rampupFactor = 1.0;
			if (fabs (speed) < speedRampupPct()) {								// if speed in the range to rampup we apply a sinus curbe to ramup

				float phaseRad = M_PI * ((fabs (speed) / speedRampupPct()) - 0.5);// we start to compute the normalized angle (speed / normalizedSpeed@5%) and translate it of -1/2PI to translate sin on 1/2 periode
				rampupFactor = ( 1 + sin(phaseRad ) ) / 2;						// sin value is -1..1 range, we translate it to 0..2 and we scale it by 2

			}

			int8_t sign = speed >= 0 ? 1 : -1;
			uint16_t coeff = speed < 0 ? effect->conditions[con_idx].negativeCoefficient : effect->conditions[con_idx].positiveCoefficient;
			force = coeff * rampupFactor * sign;

			//if there is a saturation, used it to clip result
			if (effect->conditions[con_idx].negativeSaturation !=0 || effect->conditions[con_idx].positiveSaturation !=0) {
				force = clip<int32_t, int32_t>(force, -effect->conditions[con_idx].negativeSaturation, effect->conditions[con_idx].positiveSaturation);
			}

			result_torque -= effect->filter[con_idx]->process( (((gain.friction + 1) * force) >> 8) * angle_ratio * scaler.friction);
		}

		break;
	}
	case FFB_EFFECT_DAMPER:
	{

		float speed = metrics->speed * INTERNAL_SCALER_DAMPER;
		result_torque -= effect->filter[con_idx]->process(calcConditionEffectForce(effect, speed, gain.damper, con_idx, scaler.damper, angle_ratio));

		break;
	}

	case FFB_EFFECT_INERTIA:
	{
		float accel = metrics->accel * INTERNAL_SCALER_INERTIA;
		result_torque -= effect->filter[con_idx]->process(calcConditionEffectForce(effect, accel, gain.inertia, con_idx, scaler.inertia, angle_ratio)); // Bump *60 the inertia feedback

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
int32_t EffectsCalculator::getEnvelopeMagnitude(FFB_Effect *effect)
{
	if(effect->duration == FFB_EFFECT_DURATION_INFINITE || effect->duration == 0){
		return effect->magnitude; // Effect is infinite. envelope is invalid
	}
	int32_t scaler = abs(effect->magnitude);
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
	scaler = signbit(effect->magnitude) ? -scaler : scaler; // Follow original sign of magnitude because envelope has no sign (important for constant force)
	return scaler;
}

void EffectsCalculator::setFilters(FFB_Effect *effect){

	std::function<void(std::unique_ptr<Biquad> &)> fnptr = [=](std::unique_ptr<Biquad> &filter){};

	switch (effect->type)
	{
	case FFB_EFFECT_DAMPER:
		fnptr = [=](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, this->filter.damper.freq / (float)calcfrequency, this->filter.damper.q * qfloatScaler , (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, this->filter.damper.freq / (float)calcfrequency, this->filter.damper.q * qfloatScaler, (float)0.0);
		};
		break;
	case FFB_EFFECT_FRICTION:
		fnptr = [=](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, this->filter.friction.freq / (float)calcfrequency, this->filter.friction.q * qfloatScaler, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, this->filter.friction.freq / (float)calcfrequency, this->filter.friction.q * qfloatScaler, (float)0.0);
		};
		break;
	case FFB_EFFECT_INERTIA:
		fnptr = [=](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, this->filter.inertia.freq / (float)calcfrequency, this->filter.inertia.q * qfloatScaler, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, this->filter.inertia.freq / (float)calcfrequency, this->filter.inertia.q * qfloatScaler, (float)0.0);
		};
		break;
	case FFB_EFFECT_CONSTANT:
		fnptr = [=](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, this->filter.constant.freq / (float)calcfrequency, this->filter.constant.q * qfloatScaler, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, this->filter.constant.freq / (float)calcfrequency, this->filter.constant.q * qfloatScaler, (float)0.0);
		};
		break;
	}


	for (int i=0; i<MAX_AXIS; i++) {
		fnptr(effect->filter[i]);
	}
}


void EffectsCalculator::setGain(uint8_t gain)
{
	global_gain = gain;
}

uint8_t EffectsCalculator::getGain() { return global_gain; }

void EffectsCalculator::setEffectsArray(FFB_Effect *pEffects)
{
	effects = pEffects;
}



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
		checkFilterCoeff(&(this->filter.constant), freq, q);
		updateFilterSettingsForEffects(FFB_EFFECT_CONSTANT);
	}

	if (Flash_Read(ADR_FFB_FR_FILTER, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		checkFilterCoeff(&(this->filter.friction), freq, q);
		updateFilterSettingsForEffects(FFB_EFFECT_FRICTION);
	}

	if (Flash_Read(ADR_FFB_DA_FILTER, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		checkFilterCoeff(&(this->filter.damper), freq, q);
		updateFilterSettingsForEffects(FFB_EFFECT_DAMPER);
	}

	if (Flash_Read(ADR_FFB_IN_FILTER, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		checkFilterCoeff(&(this->filter.inertia), freq, q);
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
		frictionPctSpeedToRampup = (effects & 0xff);
	}

}

// Saves parameters to flash
void EffectsCalculator::saveFlash()
{
	uint16_t filterStorage;

	// save CF biquad
	filterStorage = (uint16_t)filter.constant.freq & 0x1FF;
	filterStorage |= ( (uint16_t)filter.constant.q & 0x7F ) << 9 ;
	Flash_Write(ADR_FFB_CF_FILTER, filterStorage);

	// save Friction biquad
	filterStorage = (uint16_t)filter.friction.freq & 0x1FF;
	filterStorage |= ( (uint16_t)filter.friction.q & 0x7F ) << 9 ;
	Flash_Write(ADR_FFB_FR_FILTER, filterStorage);

	// save Damper biquad
	filterStorage = (uint16_t)filter.damper.freq & 0x1FF;
	filterStorage |= ( (uint16_t)filter.damper.q & 0x7F ) << 9 ;
	Flash_Write(ADR_FFB_DA_FILTER, filterStorage);

	// save Inertia biquad
	filterStorage = (uint16_t)filter.inertia.freq & 0x1FF;
	filterStorage |= ( (uint16_t)filter.inertia.q & 0x7F ) << 9 ;
	Flash_Write(ADR_FFB_IN_FILTER, filterStorage);

	// save the effect gain
	uint16_t effects = gain.inertia | (gain.friction << 8);
	Flash_Write(ADR_FFB_EFFECTS1, effects);

	effects = gain.spring | (gain.damper << 8);
	Flash_Write(ADR_FFB_EFFECTS2, effects);

	// save the friction rampup zone
	effects = frictionPctSpeedToRampup;
	Flash_Write(ADR_FFB_EFFECTS3, effects);

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


void EffectsCalculator::logEffectType(uint8_t type){
	if(type > 0 && type < 32){
		effects_used |= 1<<(type-1);
		if( effects_stats[type-1].nb < 65535 ) {
			effects_stats[type-1].nb ++;
		}

	}
}

void EffectsCalculator::calcStatsEffectType(uint8_t type, int16_t force){
	if(type > 0 && type < 13) {
		uint8_t arrayLocation = type - 1;
		effects_stats[arrayLocation].current = force;
		effects_stats[arrayLocation].max = std::max(effects_stats[arrayLocation].max, (int16_t)abs(force));
	}
}

/**
 * Prints a list of effects that were active at some point
 * Does not reset when an effect is deactivated
 */
std::string EffectsCalculator::listEffectsUsed(bool details){
	std::string effects_list = "";

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
			effects_list += "{\"max\":" + std::to_string(effects_stats[i].max);
			effects_list += ", \"curr\":" + std::to_string(effects_stats[i].current);
			effects_list += ", \"nb\":" + std::to_string(effects_stats[i].nb) + "}";
			firstItem = false;
		}

	}

	return effects_list.c_str();
}

/**
 * Print return all current value effect
 */
std::string EffectsCalculator::listForceEffects() {
	std::string effects_list = "";

	bool firstItem = true;
	for (int i=0; i < 12; i++) {
		if (!firstItem) effects_list += ", ";
		effects_list += std::to_string(effects_stats[i].current);
		firstItem = false;
	}

	return effects_list.c_str();
}


CommandStatus EffectsCalculator::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<EffectsCalculator_commands>(cmd.cmdId)){

	case EffectsCalculator_commands::ffbfiltercf:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter.constant.freq);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter.constant, cmd.val, filter.constant.q);
			updateFilterSettingsForEffects(FFB_EFFECT_CONSTANT);
		}
		break;
	case EffectsCalculator_commands::ffbfiltercf_q:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->qfloatScaler));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter.constant.q);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter.constant, filter.constant.freq, cmd.val);
			updateFilterSettingsForEffects(FFB_EFFECT_CONSTANT);
		}
		break;
	case EffectsCalculator_commands::effects:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(listEffectsUsed(cmd.val));
		}
		else if (cmd.type == CMDtype::set && cmd.val == 0)
		{
			effects_used = 0;
		}
		break;
	case EffectsCalculator_commands::effectsDetails:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(listEffectsUsed(true));
		}
		else if (cmd.type == CMDtype::set && cmd.val == 0)
		{
			effects_used = 0;
			for (int i=0; i<12; i++) {
				effects_stats[i].max = 0;
				effects_stats[i].current = 0;
				effects_stats[i].nb = 0;
			}
		}
		break;
	case EffectsCalculator_commands::effectsForces:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(listForceEffects());
		}
		break;
	case EffectsCalculator_commands::spring:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->scaler.spring));
		}else
			return handleGetSet(cmd, replies, this->gain.spring);
		break;
	case EffectsCalculator_commands::friction:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->scaler.friction));
		}else
			return handleGetSet(cmd, replies, this->gain.friction);
		break;
	case EffectsCalculator_commands::damper:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->scaler.damper));
		}else
			return handleGetSet(cmd, replies, this->gain.damper);
		break;
	case EffectsCalculator_commands::inertia:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->scaler.inertia));
		}else
			return handleGetSet(cmd, replies, this->gain.inertia);
		break;
	case EffectsCalculator_commands::damper_f:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter.damper.freq);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter.damper, cmd.val, filter.damper.q);
			updateFilterSettingsForEffects(FFB_EFFECT_DAMPER);
		}
		break;
	case EffectsCalculator_commands::damper_q:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->qfloatScaler));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter.damper.q);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter.damper, filter.damper.freq, cmd.val);
			updateFilterSettingsForEffects(FFB_EFFECT_DAMPER);
		}
		break;
	case EffectsCalculator_commands::friction_f:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter.friction.freq);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter.friction, cmd.val, filter.friction.q);
			updateFilterSettingsForEffects(FFB_EFFECT_FRICTION);
		}
		break;
	case EffectsCalculator_commands::friction_q:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->qfloatScaler));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter.friction.q);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter.friction, filter.friction.freq, cmd.val);
			updateFilterSettingsForEffects(FFB_EFFECT_FRICTION);
		}
		break;
	case EffectsCalculator_commands::inertia_f:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter.inertia.freq);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter.inertia, cmd.val, filter.inertia.q);
			updateFilterSettingsForEffects(FFB_EFFECT_INERTIA);
		}
		break;
	case EffectsCalculator_commands::inertia_q:
		if(cmd.type == CMDtype::info){
			replies.emplace_back("scale:"+std::to_string(this->qfloatScaler));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(filter.inertia.q);
		}
		else if (cmd.type == CMDtype::set)
		{
			checkFilterCoeff(&filter.inertia, filter.inertia.freq, cmd.val);
			updateFilterSettingsForEffects(FFB_EFFECT_INERTIA);
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

