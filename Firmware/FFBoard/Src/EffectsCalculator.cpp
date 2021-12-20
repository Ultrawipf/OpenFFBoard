/*
 * EffectsCalculator.cpp
 *
 *  Created on: 27.01.21
 *      Author: Jon Lidgard
 */

#include <stdint.h>
#include "math.h"
#include "EffectsCalculator.h"
//#include "global_callbacks.h"
#include "Axis.h"

#define X_AXIS_ENABLE 1
#define Y_AXIS_ENABLE 2
#define Z_AXIS_ENABLE 4
//#define DEG_TO_RAD ((float)((float)M_PI / 180.0))
#define DIRECTION_ENABLE (1 << MAX_AXIS)

#define EFFECT_STATE_INACTIVE 0

ClassIdentifier EffectsCalculator::info = {
		  .name = "Effects" ,
		  .id	= CLSID_EFFECTSCALC,
		  .hidden = true
};
const ClassIdentifier EffectsCalculator::getInfo(){
	return info;
}


EffectsCalculator::EffectsCalculator() : CommandHandler("fx", CLSID_EFFECTSCALC)
{
	restoreFlash();

	CommandHandler::registerCommands();
	registerCommand("filterCfFreq", EffectsCalculator_commands::ffbfiltercf, "Constant force filter frequency", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("filterCfQ", EffectsCalculator_commands::ffbfiltercf_q, "Constant force filter Q-factor", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("spring", EffectsCalculator_commands::spring, "Spring gain", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("friction", EffectsCalculator_commands::friction, "Friction gain", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("damper", EffectsCalculator_commands::damper, "Damper gain", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("inertia", EffectsCalculator_commands::inertia, "Inertia gain", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("effects", EffectsCalculator_commands::effects, "List effects", CMDFLAG_GET | CMDFLAG_STR_ONLY);
}

EffectsCalculator::~EffectsCalculator()
{
	// Remove from global list when deleted
	//	removeEffectsCalculator();
}

// bool EffectsCalculator::processHidCommand(HID_Custom_Data_t* data) {
//  return false;
// }

bool EffectsCalculator::isActive()
{
	return effects_active;
}
void EffectsCalculator::setActive(bool active)
{
	effects_active = active;
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

		// Effect activated and not infinite
		if (effect->state != EFFECT_STATE_INACTIVE && effect->duration != FFB_EFFECT_DURATION_INFINITE){
			// Start delay not yet reached
			if(HAL_GetTick() < effect->startTime){
				continue;
			}
			// If effect has expired make inactive
			if (HAL_GetTick() > effect->startTime + effect->duration)
			{
				effect->state = EFFECT_STATE_INACTIVE;
			}
		}

		// Filter out inactive effects
		if (effect->state == EFFECT_STATE_INACTIVE)
		{
			continue;
		}


		if (effect->conditionsCount == 0) {
			forceVector = calcNonConditionEffectForce(effect);
		}

		if (effect->enableAxis == DIRECTION_ENABLE || (effect->enableAxis & X_AXIS_ENABLE))
		{
			forceX += calcComponentForce(effect, forceVector, axes, 0);
			forceX = clip<int32_t, int32_t>(forceX, -0x7fff, 0x7fff); // Clip
		}
		if (validY && ((effect->enableAxis == DIRECTION_ENABLE) || (effect->enableAxis & Y_AXIS_ENABLE)))
		{
			forceY += calcComponentForce(effect, forceVector, axes, 1);
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
	switch (effect->type){

	case FFB_EFFECT_CONSTANT:
	{ // Constant force is just the force
		force_vector = ((int32_t)effect->magnitude * (int32_t)(1 + effect->gain)) >> 8;
		// Optional filtering to reduce spikes
		if (cfFilter_f < calcfrequency / 2 && cfFilter_f != 0 )
		{
			force_vector = effect->filter[0]->process(force_vector);
		}
		break;
	}

	case FFB_EFFECT_RAMP:
	{
		uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
		int32_t duration = effect->duration;
		float force = (int32_t)effect->startLevel + ((int32_t)elapsed_time * (effect->endLevel - effect->startLevel)) / duration;
		force_vector = (int32_t)(force * (1 + effect->gain)) >> 8;
		break;
	}

	case FFB_EFFECT_SQUARE:
	{
		uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
		int32_t force = ((elapsed_time + effect->phase) % ((uint32_t)effect->period + 2)) < (uint32_t)(effect->period + 2) / 2 ? -effect->magnitude : effect->magnitude;
		force_vector = force + effect->offset;
		break;
	}

	case FFB_EFFECT_TRIANGLE:
	{
		int32_t force = 0;
		int32_t offset = effect->offset;
		int32_t magnitude = effect->magnitude;
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
		float magnitude = effect->magnitude;
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
		float magnitude = effect->magnitude;
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
		uint32_t t = HAL_GetTick() - effect->startTime;
		float freq = 1.0f / (float)(std::max<uint16_t>(effect->period, 2));
		float phase = (float)effect->phase / (float)35999; //degrees
		float sine = sinf(2.0 * (float)M_PI * (t * freq + phase)) * effect->magnitude;
		force_vector = (int32_t)(effect->offset + sine);
		break;
	}
	default:
		break;
	}
	if(effect->useEnvelope) {
		force_vector = applyEnvelope(effect, (int32_t)force_vector);
	}
	return force_vector;
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
	float scaleSpeed = 40;//axes[axis]->getSpeedScalerNormalized(); // TODO decide if scalers are useful or not
	float scaleAccel = 40;//axes[axis]->getAccelScalerNormalized();

	if (effect->enableAxis == DIRECTION_ENABLE)
	{
		direction = effect->directionX;
		if (effect->conditionsCount > 1)
		{
			con_idx = axis;
		}
	}
	else
	{
		direction = axis == 0 ? effect->directionX : effect->directionY;
		con_idx = axis;
	}

	//bool useForceDirectionForConditionEffect = (effect->enableAxis == DIRECTION_ENABLE && axisCount > 1 && effect->conditionsCount == 1);
	bool rotateConditionForce = (axisCount > 1 && effect->conditionsCount < axisCount);
	float angle = ((float)direction * (2*M_PI) / 36000.0);
	float angle_ratio = axis == 0 ? sin(angle) : -1 * cos(angle);
	angle_ratio = rotateConditionForce ? angle_ratio : 1.0;

	switch (effect->type)
	{
	case FFB_EFFECT_CONSTANT:
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
		result_torque -= calcConditionEffectForce(effect, pos, gain.spring, con_idx, spring_scaler, angle_ratio);
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
		float speed = metrics->speed * scaleSpeed;//effect->filter[con_idx]->process()

		int16_t offset = effect->conditions[con_idx].cpOffset;
		int16_t deadBand = effect->conditions[con_idx].deadBand;
		int32_t force = 0;

		// Effect is only active outside deadband + offset
		if (abs((int32_t)speed - offset) > deadBand){

			// remove offset/deadband from metric to compute force
			speed -= (offset + (deadBand * (speed < offset ? -1 : 1)) );

			// check if speed is in the 0..x% to rampup, if is this range, apply a sinusoidale function to smooth the torque (slow near 0, slow around the X% rampup
			float rampupFactor = 1.0;
			if (fabs (speed) < speedRampupPct) {								// if speed in the range to rampup we apply a sinus curbe to ramup

				float phaseRad = M_PI * ((fabs (speed) / speedRampupPct) - 0.5);// we start to compute the normalized angle (speed / normalizedSpeed@5%) and translate it of -1/2PI to translate sin on 1/2 periode
				rampupFactor = ( 1 + sin(phaseRad ) ) / 2;						// sin value is -1..1 range, we translate it to 0..2 and we scale it by 2

			}

			int8_t sign = speed >= 0 ? 1 : -1;
			uint16_t coeff = speed < 0 ? effect->conditions[con_idx].negativeCoefficient : effect->conditions[con_idx].positiveCoefficient;
			force = coeff * rampupFactor * sign;

			//if there is a saturation, used it to clip result
			if (effect->conditions[con_idx].negativeSaturation !=0 || effect->conditions[con_idx].positiveSaturation !=0) {
				force = clip<int32_t, int32_t>(force, -effect->conditions[con_idx].negativeSaturation, effect->conditions[con_idx].positiveSaturation);
			}

//			static int32_t last_force = 0;
//
//			// if there is 2 successive torque with a different direction, we ignore the first one to remove oscillation
//			if (last_force * force >=0) {
//				force = ((gain.friction + 1) * force) >> 7;
//				result_torque -=  force * angle_ratio;
//			}
//			last_force = force;
			result_torque -= effect->filter[con_idx]->process( (((gain.friction + 1) * force) >> 8) * angle_ratio * friction_scaler);
		}
//			float accel = metrics->accel * scaleAccel;
//			result_torque -= calcConditionEffectForce(effect, accel, gain.friction, con_idx, friction_scaler, angle_ratio);

		break;
	}
	case FFB_EFFECT_DAMPER:
	{

		float speed = metrics->speed * scaleSpeed;
		result_torque -= effect->filter[con_idx]->process(calcConditionEffectForce(effect, speed, gain.damper, con_idx, damper_scaler, angle_ratio));

		break;
	}

	case FFB_EFFECT_INERTIA:
	{
		float accel = metrics->accel* scaleAccel;
		result_torque -= effect->filter[con_idx]->process(calcConditionEffectForce(effect, accel, gain.inertia, con_idx, inertia_scaler, angle_ratio)); // Bump *60 the inertia feedback

		break;
	}

	default:
		// Unsupported effect
		break;
	}
	return (result_torque * (global_gain+1)) >> 8; // Apply global gain
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

// Check correct levels. Looks reasonable compared with fedit preview
int32_t EffectsCalculator::applyEnvelope(FFB_Effect *effect, int32_t value)
{
	int32_t magnitude = (effect->magnitude);
	int32_t attackLevel = (effect->attackLevel);
	int32_t fadeLevel = (effect->fadeLevel);
	int32_t newValue = magnitude;
	uint32_t elapsed_time = HAL_GetTick() - effect->startTime;
	if (elapsed_time < effect->attackTime)
	{
		newValue = (magnitude - attackLevel) * elapsed_time;
		newValue /= (int32_t)effect->attackTime;
		newValue += attackLevel;
	}
	if (effect->duration != FFB_EFFECT_DURATION_INFINITE &&
		elapsed_time > (effect->duration - effect->fadeTime))
	{
		newValue = (magnitude - fadeLevel) * (effect->duration - elapsed_time);
		newValue /= (int32_t)effect->fadeTime;
		newValue += fadeLevel;
	}

	newValue *= value;
	newValue /= 0x7fff; // 16 bit
	return newValue;
}

void EffectsCalculator::setFilters(FFB_Effect *effect){

	std::function<void(std::unique_ptr<Biquad> &)> fnptr = [=](std::unique_ptr<Biquad> &filter){};

	switch (effect->type)
	{
	case FFB_EFFECT_DAMPER:
		fnptr = [=](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, (float)damper_f / (float)calcfrequency, damper_q, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, (float)damper_f / (float)calcfrequency, damper_q, (float)0.0);
		};
		break;
	case FFB_EFFECT_FRICTION:
		fnptr = [=](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, (float)friction_f / (float)calcfrequency, friction_q, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, (float)friction_f / (float)calcfrequency, friction_q, (float)0.0);
		};
		break;
	case FFB_EFFECT_INERTIA:
		fnptr = [=](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, (float)inertia_f / (float)calcfrequency, inertia_q, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, (float)inertia_f / (float)calcfrequency, inertia_q, (float)0.0);
		};
		break;
	case FFB_EFFECT_CONSTANT:
		fnptr = [=](std::unique_ptr<Biquad> &filter){
			if (filter != nullptr)
				filter->setBiquad(BiquadType::lowpass, (float)cfFilter_f / (float)calcfrequency, cfFilter_qfloatScaler * (cfFilter_q+1), (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, (float)cfFilter_f / (float)calcfrequency, cfFilter_qfloatScaler * (cfFilter_q+1), (float)0.0);
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
	uint16_t filter;
	if (Flash_Read(ADR_CF_FILTER, &filter))
	{
		this->cfFilter_f = filter & 0x1FF;
		this->cfFilter_q = (filter >> 9) & 0x7F;
	}
	setCfFilter(this->cfFilter_f,this->cfFilter_q);

	uint16_t effects = 0;
	if(Flash_Read(ADR_AXIS_EFFECTS1, &effects)){
		gain.friction = (effects >> 8) & 0xff;
		gain.inertia = (effects & 0xff);
	}
	if(Flash_Read(ADR_AXIS_EFFECTS2, &effects)){
		gain.damper = (effects >> 8) & 0xff;
		gain.spring = (effects & 0xff);
	}

}

// Saves parameters to flash
void EffectsCalculator::saveFlash()
{
	uint16_t cffilter = (cfFilter_f & 0x1FF) | ((cfFilter_q & 0x7F) << 9);
	Flash_Write(ADR_CF_FILTER, cffilter);
	uint16_t effects = gain.inertia | (gain.friction << 8);
	Flash_Write(ADR_AXIS_EFFECTS1, effects);
	effects = gain.spring | (gain.damper << 8);
	Flash_Write(ADR_AXIS_EFFECTS2, effects);

}

void EffectsCalculator::setCfFilter(uint32_t freq,uint8_t q)
{
	this->cfFilter_q = clip<uint8_t, uint8_t>(q,0,127);

	if(freq == 0){
		freq = calcfrequency / 2;
	}
	cfFilter_f = clip<uint32_t, uint32_t>(freq, 1, (calcfrequency / 2));
	float f = (float)cfFilter_f / (float)calcfrequency;

	for (uint8_t i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i].type == FFB_EFFECT_CONSTANT)
		{
			effects[i].filter[0]->setFc(f);
			effects[i].filter[0]->setQ(cfFilter_qfloatScaler * (cfFilter_q+1));
		}
	}
}

void EffectsCalculator::logEffectType(uint8_t type){
	if(type > 0 && type < 32){
		effects_used |= 1<<(type-1);
	}
}

/**
 * Prints a list of effects that were active at some point
 * Does not reset when an effect is deactivated
 */
std::string EffectsCalculator::listEffectsUsed(){
	std::string effects_list = "";
	static const char *effects[12] = {"Constant,","Ramp,","Square,","Sine,","Triangle,","Sawtooth Up,","Sawtooth Down,","Spring,","Damper,","Inertia,","Friction,","Custom,"};

	if(effects_used == 0){
		return "None";
	}

	for (int i=0;i < 12; i++) {
		if((effects_used >> i) & 1) {
			effects_list += effects[i];
		}
	}
	effects_list.pop_back();
	return effects_list;
}


CommandStatus EffectsCalculator::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<EffectsCalculator_commands>(cmd.cmdId)){

	case EffectsCalculator_commands::ffbfiltercf:
		if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(cfFilter_f));
		}
		else if (cmd.type == CMDtype::set)
		{
			setCfFilter(cmd.val,this->cfFilter_q);
		}
		break;

	case EffectsCalculator_commands::ffbfiltercf_q:
		if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(cfFilter_q));
		}
		else if (cmd.type == CMDtype::set)
		{
			setCfFilter(this->cfFilter_f,cmd.val);
		}

		break;

	case EffectsCalculator_commands::effects:
		if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(listEffectsUsed()));
		}
		else if (cmd.type == CMDtype::set)
		{
			effects_used = 0;
		}
		break;
	case EffectsCalculator_commands::spring:
		return handleGetSet(cmd, replies, this->gain.spring);
	case EffectsCalculator_commands::friction:
		return handleGetSet(cmd, replies, this->gain.friction);
	case EffectsCalculator_commands::damper:
		return handleGetSet(cmd, replies, this->gain.damper);
	case EffectsCalculator_commands::inertia:
		return handleGetSet(cmd, replies, this->gain.inertia);

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}

