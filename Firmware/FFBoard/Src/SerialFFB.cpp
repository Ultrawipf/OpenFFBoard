/*
 * SerialFFB.cpp
 *
 *  Created on: 30.11.2022
 *      Author: Yannick
 */

#include "SerialFFB.h"

ClassIdentifier SerialFFB::info = {
		  .name = "Effects manager" ,
		  .id	= CLSID_EFFECTSMGR,
		  .visibility = ClassVisibility::visible
};
const ClassIdentifier SerialFFB::getInfo(){
	return info;
}

/**
 * Creates an interface to control standard PID effects via commands
 */
SerialFFB::SerialFFB(std::shared_ptr<EffectsCalculator> ec,uint8_t instance) : CommandHandler("fxm", CLSID_EFFECTSMGR,instance), effects_calc(ec) {

	CommandHandler::registerCommands();
	registerCommand("ffbstate", SerialEffects_commands::ffbstate, "FFB active", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("type", SerialEffects_commands::fxtype, "Effect type", CMDFLAG_GETADR);
	registerCommand("reset", SerialEffects_commands::ffbreset, "Reset all effects or effect adr", CMDFLAG_GET | CMDFLAG_GETADR);
	registerCommand("new", SerialEffects_commands::newEffect, "Create new effect of type val. Returns index or -1 on err", CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("mag", SerialEffects_commands::fxmagnitude, "16b magnitude of non cond. effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("state", SerialEffects_commands::fxstate, "Enable/Disable effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("period", SerialEffects_commands::fxperiod, "Period of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("duration", SerialEffects_commands::fxduration, "Duration of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("offset", SerialEffects_commands::fxoffset, "Offset of cond. effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("deadzone", SerialEffects_commands::fxdeadzone, "Deadzone of cond. effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("sat", SerialEffects_commands::fxsat, "Saturation of cond. effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("coeff", SerialEffects_commands::fxcoeff, "Coefficient of cond. effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("axisgain", SerialEffects_commands::fxaxisgain, "Gain for this axis (instance) 16b", CMDFLAG_SETADR | CMDFLAG_GETADR);
}

SerialFFB::~SerialFFB() {
	reset_ffb();
}


bool SerialFFB::getFfbActive(){
	return this->effects_calc->isActive();
}

/**
 * Resets all effects and disables ffb
 */
void SerialFFB::reset_ffb(){
	for(uint8_t i=0;i<EffectsCalculator::max_effects;i++){
		effects_calc->free_effect(i);
	}
	set_FFB(false);
	set_gain(255);
}
/**
 * Enables or disables FFB actuator
 */
void SerialFFB::set_FFB(bool state){
	this->effects_calc->setActive(state);
}
/**
 * Changes the global gain scaler
 */
void SerialFFB::set_gain(uint8_t gain){
	effects_calc->setGain(gain); // Global gain
}

/**
 * Takes an effect type and allocates it in the effects array
 * Returns the index where the effect was created or -1 if it can not be created
 */
int32_t SerialFFB::newEffect(uint8_t effectType){
	int32_t idx = this->effects_calc->find_free_effect(effectType);
	if(idx >= 0){
		std::unique_ptr<Effect> new_effect = Effect::create(effectType);
		if(!new_effect) return -1;
		new_effect->setDuration(FFB_EFFECT_DURATION_INFINITE);
		new_effect->setAxisMagnitude(std::min(this->getCommandHandlerInstance(),(uint8_t)MAX_AXIS), 1);
		new_effect->setUseSingleCondition(false);
		
		FFB_Effect_Condition cond;
		// default cond set zero
		cond.cpOffset = 0; cond.deadBand = 0; cond.negativeCoefficient = 0; cond.positiveCoefficient = 0; cond.negativeSaturation = 0; cond.positiveSaturation = 0;
		// Need proper setCondition but for simplicity we will just call it manually later
		this->effects_calc->setFilters(new_effect.get());
		this->effects_calc->logEffectType(effectType,false);
		effects_calc->storeEffect(idx, std::move(new_effect));
	}
	return idx;
}

/**
 * Changes magnitude of non conditional effects (Constant, ramp, square, triangle, sawtooth)
 */
void SerialFFB::setMagnitude(uint8_t idx,int16_t magnitude){
	if(idx >= EffectsCalculator::max_effects || !effects_calc->getEffect(idx)){
		return;
	}
	Effect* effect = effects_calc->getEffect(idx);
	effect->setMagnitude(magnitude);

	if(effect->getType() == FFB_EFFECT_CONSTANT){
		EffectsControlItf::cfUpdateEvent();
	}
}

/**
 * Enables or disables an effect
 */
void SerialFFB::setEffectState(uint8_t id, bool state){
	if(id >= EffectsCalculator::max_effects || !effects_calc->getEffect(id)){
		return;
	}
	if(state){
		effects_calc->getEffect(id)->setStartTime(HAL_GetTick() + effects_calc->getEffect(id)->getStartDelay());
	}
	effects_calc->getEffect(id)->setState(state ? 1 : 0);
}

void SerialFFB::updateSamplerate(float newSamplerate){
	effects_calc->updateSamplerate(newSamplerate);
}

CommandStatus SerialFFB::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	CommandStatus status = CommandStatus::OK;
	EffectsControlItf::fxUpdateEvent();

	switch(static_cast<SerialEffects_commands>(cmd.cmdId)){
	case SerialEffects_commands::ffbstate:
		return handleGetFuncSetFunc(cmd, replies, &SerialFFB::getFfbActive, &SerialFFB::set_FFB, this);

	case SerialEffects_commands::ffbreset:
		if(cmd.type == CMDtype::get){
			reset_ffb();
		}else if(cmd.type == CMDtype::getat){
			effects_calc->free_effect(cmd.adr);
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::newEffect:
		if(cmd.type == CMDtype::set){
			replies.emplace_back(newEffect(cmd.val));
		}else if(cmd.type == CMDtype::info){
			replies.emplace_back("Constant=1,Ramp=2,Square=3,Sine=4,Triangle=5,Sawtooth Up=6,Sawtooth Down=7,Spring=8,Damper=9,Inertia=10,Friction=11");
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxmagnitude:
	case SerialEffects_commands::fxperiod:
	case SerialEffects_commands::fxduration:
	case SerialEffects_commands::fxoffset:
	case SerialEffects_commands::fxdeadzone:
	case SerialEffects_commands::fxaxisgain:
	case SerialEffects_commands::fxsat:
	case SerialEffects_commands::fxcoeff: {
		if (cmd.adr >= EffectsCalculator::max_effects) {
			return CommandStatus::ERR;
		}
		Effect* effect = effects_calc->getEffect(cmd.adr);
		if (!effect) {
			return CommandStatus::ERR;
		}

		EffectParameter param;
		switch (static_cast<SerialEffects_commands>(cmd.cmdId)) {
			case SerialEffects_commands::fxmagnitude: param = EffectParameter::Magnitude; break;
			case SerialEffects_commands::fxperiod:    param = EffectParameter::Period; break;
			case SerialEffects_commands::fxduration:  param = EffectParameter::Duration; break;
			case SerialEffects_commands::fxoffset:    param = EffectParameter::Offset; break;
			case SerialEffects_commands::fxdeadzone:  param = EffectParameter::Deadzone; break;
			case SerialEffects_commands::fxaxisgain:  param = EffectParameter::AxisGain; break;
			case SerialEffects_commands::fxsat:       param = EffectParameter::Saturation; break;
			case SerialEffects_commands::fxcoeff:     param = EffectParameter::Coefficient; break;
			default: return CommandStatus::ERR;
		}

		uint8_t axis = getCommandHandlerInstance();

		if (cmd.type == CMDtype::setat) {
			effect->setParameter(param, axis, cmd.val);
			if (param == EffectParameter::Magnitude && effect->getType() == FFB_EFFECT_CONSTANT) {
				EffectsControlItf::cfUpdateEvent();
			}
			return CommandStatus::OK;
		} else if (cmd.type == CMDtype::getat) {
			replies.emplace_back(effect->getParameter(param, axis));
			return CommandStatus::OK;
		} else {
			return CommandStatus::ERR;
		}
		break;
	}

	case SerialEffects_commands::fxstate:
		if(cmd.adr < EffectsCalculator::max_effects && effects_calc->getEffect(cmd.adr)){
			if(cmd.type == CMDtype::setat){
				setEffectState(cmd.adr,cmd.val);
			}else if(cmd.type == CMDtype::getat){
				replies.emplace_back(effects_calc->getEffect(cmd.adr)->getState());
			}else{
				return CommandStatus::ERR;
			}
		}else{
			return CommandStatus::ERR;
		}

		break;

	case SerialEffects_commands::fxtype:
		if(cmd.adr < EffectsCalculator::max_effects && cmd.type == CMDtype::getat && effects_calc->getEffect(cmd.adr)){
			replies.emplace_back(effects_calc->getEffect(cmd.adr)->getType());
		}else
			return CommandStatus::ERR;
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return status;
}
