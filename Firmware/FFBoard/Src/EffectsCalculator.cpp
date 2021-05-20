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
		.name 	= "NONE" ,
			 .id	= 0,
			 .unique = '0',
			 .hidden = true
};
const ClassIdentifier EffectsCalculator::getInfo(){
	return info;
}


EffectsCalculator::EffectsCalculator()
{
	restoreFlash();
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

		if (effect->enableAxis == DIRECTION_ENABLE || effect->enableAxis & X_AXIS_ENABLE)
		{
			forceX += calcComponentForce(effect, forceVector, axes[0]->getMetrics(), 0, axisCount);
			forceX = clip<int32_t, int32_t>(forceX, -0x7fff, 0x7fff); // Clip
		}
		if (validY && ((effect->enableAxis == DIRECTION_ENABLE) || effect->enableAxis & Y_AXIS_ENABLE))
		{
			forceY += calcComponentForce(effect, forceVector, axes[1]->getMetrics(), 1, axisCount);
			forceY = clip<int32_t, int32_t>(forceY, -0x7fff, 0x7fff); // Clip
		}

	}

	axes[0]->setEffectTorque(forceX);
	if (validY)
	{
		axes[1]->setEffectTorque(forceY);
	}
}

int32_t EffectsCalculator::calcNonConditionEffectForce(FFB_Effect *effect) {
	int32_t force_vector = 0;
	switch (effect->type){

	case FFB_EFFECT_CONSTANT:
	{ // Constant force is just the force
		force_vector = ((int32_t)effect->magnitude * (int32_t)(1 + effect->gain)) >> 8;
		// Optional filtering to reduce spikes
		if (cfFilter_f < calcfrequency / 2)
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

int32_t EffectsCalculator::calcComponentForce(FFB_Effect *effect, int32_t forceVector, metric_t *metrics, uint8_t axis, uint8_t axisCount)
{
	int32_t result_torque = 0;
	uint16_t direction;
	uint8_t con_idx = 0; // condition block index

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
		float metric = metrics->pos;
		result_torque -= calcConditionEffectForce(effect, metric, gain.spring,
									   con_idx, 0.0004f, angle_ratio);
		break;
	}

	case FFB_EFFECT_FRICTION:
	{
//		effect->conditions[con_idx].negativeSaturation = FRICTION_SATURATION;
//		effect->conditions[con_idx].positiveSaturation = FRICTION_SATURATION;
		float metric = effect->filter[con_idx]->process(metrics->speed) * .25;
		result_torque -= calcConditionEffectForce(effect, metric, gain.friction,
											   con_idx, .08f, angle_ratio);
		break;
	}
	case FFB_EFFECT_DAMPER:
	{
		float metric = effect->filter[con_idx]->process(metrics->speed) * .0625f;
		result_torque -= calcConditionEffectForce(effect, metric, gain.damper,
									   con_idx, 0.6f, angle_ratio);
		break;
	}

	case FFB_EFFECT_INERTIA:
	{
//		effect->conditions[con_idx].negativeSaturation = INERTIA_SATURATION;
//		effect->conditions[con_idx].positiveSaturation = INERTIA_SATURATION;
		float metric = effect->filter[con_idx]->process(metrics->accel*4);
		result_torque -= calcConditionEffectForce(effect, metric, gain.inertia,
									   con_idx, 0.5f, angle_ratio);
		break;
	}

	default:
		// Unsupported effect
		break;
	}
	return (result_torque * (global_gain+1)) >> 8; // Apply global gain
}

int32_t EffectsCalculator::calcConditionEffectForce(FFB_Effect *effect, float  metric, uint8_t gain,
										 uint8_t idx, float scale, float angle_ratio)
{
	int16_t offset = effect->conditions[idx].cpOffset;
	int16_t deadBand = effect->conditions[idx].deadBand;
	int32_t force = 0;
	if (abs(metric - offset) > deadBand)
	{
		if (metric < offset)
		{ // Deadband side
			force = clip<int32_t, int32_t>(((float)effect->conditions[idx].negativeCoefficient *
											scale * (float)(metric - (offset - deadBand))),
										   -effect->conditions[idx].negativeSaturation,
										   effect->conditions[idx].positiveSaturation);
		}
		else
		{
			force = clip<int32_t, int32_t>(((float)effect->conditions[idx].positiveCoefficient *
											scale * (float)(metric - (offset + deadBand))),
										   -effect->conditions[idx].negativeSaturation,
										   effect->conditions[idx].positiveSaturation);
		}
	}
	force = ((gain+1) * force) >> 8;
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
				filter->setBiquad(BiquadType::lowpass, (float)cfFilter_f / (float)calcfrequency, cfFilter_q, (float)0.0);
			else
				filter = std::make_unique<Biquad>(BiquadType::lowpass, (float)cfFilter_f / (float)calcfrequency, cfFilter_q, (float)0.0);
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
	uint16_t filter = calcfrequency / 2; // 500 = off
	if (Flash_Read(ADR_CF_FILTER, &filter))
	{
		setCfFilter(filter);
	}
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
	Flash_Write(ADR_CF_FILTER, cfFilter_f);
	uint16_t effects = gain.inertia | (gain.friction << 8);
	Flash_Write(ADR_AXIS_EFFECTS1, effects);
	effects = gain.spring | (gain.damper << 8);
	Flash_Write(ADR_AXIS_EFFECTS2, effects);

}

void EffectsCalculator::setCfFilter(uint32_t freq)
{
	if(freq == 0){
		freq = calcfrequency / 2;
	}
	cfFilter_f = clip<uint32_t, uint32_t>(freq, 1, calcfrequency / 2);
	float f = (float)cfFilter_f / (float)calcfrequency;

	for (uint8_t i = 0; i < MAX_EFFECTS; i++)
	{
		if (effects[i].type == FFB_EFFECT_CONSTANT)
		{
			effects[i].filter[0]->setFc(f);
		}
	}
}

float EffectsCalculator::getCfFilterFreq() { return cfFilter_f; }


void EffectsCalculator::logEffectType(uint8_t type){
	if(type > 0 && type < 32){
		effects_used |= 1<<(type-1);
	}
}

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


void EffectsCalculator::processHidCommand(HID_Custom_Data_t* data) {

	switch (data->cmd&0x3F){
		case HID_CMD_FFB_FRICTION:
			if(data->type == HidCmdType::write) {
				gain.friction = data->data;
			}
			else if(data->type == HidCmdType::request){
				data->data = gain.friction;
			}
			break;

		default:

			break;
		}
	return;
}

ParseStatus EffectsCalculator::command(ParsedCommand *cmd, std::string *reply)
{
	ParseStatus flag = ParseStatus::OK;
	if (cmd->cmd == "ffbfiltercf")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string((int)getCfFilterFreq());
		}
		else if (cmd->type == CMDtype::set)
		{
			setCfFilter(cmd->val);
		}
	}else if (cmd->cmd == "effects")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += listEffectsUsed();
		}
		else if (cmd->type == CMDtype::set)
		{
			effects_used = 0;
		}
		else
		{
			*reply += "List effects used.";
		}
	}
	else if(cmd->cmd == "spring")
	{
		if(cmd->type == CMDtype::get)
		{
			*reply+=std::to_string(gain.spring);
		}
		else if(cmd->type == CMDtype::set)
		{
			gain.spring = cmd->val;
		}
	}
	else if(cmd->cmd == "friction")
	{
		if(cmd->type == CMDtype::get)
		{
			*reply+=std::to_string(gain.friction);
		}
		else if(cmd->type == CMDtype::set)
		{
			gain.friction = cmd->val;
		}
	}
	else if(cmd->cmd == "damper")
	{
		if(cmd->type == CMDtype::get)
		{
			*reply+=std::to_string(gain.damper);
		}
		else if(cmd->type == CMDtype::set)
		{
			gain.damper = cmd->val;
		}
	}
	else if(cmd->cmd == "inertia")
	{
		if(cmd->type == CMDtype::get)
		{
			*reply+=std::to_string(gain.inertia);
		}
		else if(cmd->type == CMDtype::set)
		{
			gain.inertia = cmd->val;
		}
	}else{
		flag = ParseStatus::NOT_FOUND;
	}
	return flag;
};

