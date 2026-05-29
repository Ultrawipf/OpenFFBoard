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
#include "arm_math_types.h"
#include "dsp/fast_math_functions.h"
#include "dsp/interpolation_functions.h"
#define MATH_PI PI
#define MATH_SIN(x) arm_sin_f32(x)
#else
#define MATH_PI M_PI
#define MATH_SIN(x) sinf(x)
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
		effects_stats[i].current.fill(0); // Reset active effect forces
		effects_statslast[i].current.fill(0);
	}
	setClipLed(active);
}

void EffectsCalculator::updateSamplerate(float newSamplerate){
	this->calcfrequency = newSamplerate;
	for(auto& effect : this->effects){
		if(effect && effect->getFilter(0)){ // Update filters if effect has filters
			setFilters(effect.get());
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

/**
 * Calculates the resulting torque for FFB effects
 * Takes current position input scaled from -0x7fff to 0x7fff
 * Outputs a torque value from -0x7fff to 0x7fff (not yet clipped)
 */
void EffectsCalculator::calculateEffects(std::vector<std::unique_ptr<Axis>> &axes)
{
	int axisCount = axes.size();
	int32_t forces[MAX_AXIS] = {0};

	if(isActive()){
		for (uint8_t i = 0; i < effects_stats.size(); i++)
		{
			effects_stats[i].current.fill(0); // Reset active effect forces
		}

		for (auto& effect : effects)
		{
			if (effect && effect->getState() != EFFECT_STATE_INACTIVE)
			{
				for(uint8_t axis=0 ; axis < axisCount ; axis++)
				{
					int32_t axisforce = effect->processForce(axis, axes[axis]->getMetrics(), global_gain);
					calcStatsEffectType(effect->getType(), axisforce, axis);
					forces[axis] += axisforce;
				}
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
	if(Flash_Read(ADR_FFB_RECONSTRUCTION_FILTER, &effects)){
		reconFilterMode = (ReconFilterMode)(effects & 0x03);
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
	effects = (uint16_t)(reconFilterMode);
	Flash_Write(ADR_FFB_RECONSTRUCTION_FILTER, effects);
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
	for (auto& effect : effects)
	{
		if (effect && effect->getType() == type_effect)
		{
			setFilters(effect.get());
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
			effects_stats[type-1].current.fill(0);
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
	if(idx < effects.size()){
		if(effects[idx]) {
			logEffectType(effects[idx]->getType(), true); // Effect off
			effects[idx].reset();
		}
	}
}

/**
 * Will return the first effect index which is empty or -1 if none found
 */
int32_t EffectsCalculator::find_free_effect(uint8_t type){
	if(type > FFB_EFFECT_NONE && type < FFB_EFFECT_CUSTOM+1){ // Check if it is a valid effect type
		for(uint8_t i=0;i<effects.size();i++){
			if(!effects[i]){
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

void EffectsCalculator::setFilters(Effect *effect){
	uint32_t freq = 0;
	uint8_t q = 0;
	bool apply = true;

	switch (effect->getType())
	{
	case FFB_EFFECT_DAMPER:
		freq = this->filter[filterProfileId].damper.freq;
		q = this->filter[filterProfileId].damper.q;
		break;
	case FFB_EFFECT_FRICTION:
		freq = this->filter[filterProfileId].friction.freq;
		q = this->filter[filterProfileId].friction.q;
		break;
	case FFB_EFFECT_INERTIA:
		freq = this->filter[filterProfileId].inertia.freq;
		q = this->filter[filterProfileId].inertia.q;
		break;
	case FFB_EFFECT_CONSTANT:
		freq = this->filter[0].constant.freq;
		q = this->filter[0].constant.q;
		break;
	default:
		apply = false;
		break;
	}

	if (apply) {
		float freq_f = freq / (float)calcfrequency;
		float q_f = q * qfloatScaler;
		for (int i=0; i<MAX_AXIS; i++) {
			effect->setFilter(i, freq_f, q_f, 0.0f);
		}
	}
}
