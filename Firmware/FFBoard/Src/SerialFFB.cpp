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
SerialFFB::SerialFFB(std::shared_ptr<EffectsCalculator> ec,uint8_t instance) : CommandHandler("fxm", CLSID_EFFECTSMGR,instance), effects_calc(ec), effects(ec->effects) {

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
	for(uint8_t i=0;i<effects.size();i++){
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
		// Allocate effect
		effects[idx].type = effectType;
		effects[idx].duration = FFB_EFFECT_DURATION_INFINITE;
		effects[idx].axisMagnitudes[std::min(this->getCommandHandlerInstance(),(uint8_t)MAX_AXIS)] = 1;
		effects[idx].useSingleCondition = false;
		for(auto &cond : effects[idx].conditions){ // Set default conditions
			cond = defaultCond;
		}
		this->effects_calc->logEffectType(effectType,false);
	}
	return idx;
}

/**
 * Changes magnitude of non conditional effects (Constant, ramp, square, triangle, sawtooth)
 */
void SerialFFB::setMagnitude(uint8_t idx,int16_t magnitude){
	if(idx > effects.size()){
		return;
	}
	FFB_Effect* effect = &effects[idx];
	effect->magnitude = magnitude;

	if(effect->type == FFB_EFFECT_CONSTANT){
		EffectsControlItf::cfUpdateEvent();
	}

	// Compatibility for conditional effects... Recommended to use the set coefficient functions instead.
	effects[idx].conditions[getCommandHandlerInstance()].negativeCoefficient;
	effects[idx].conditions[getCommandHandlerInstance()].positiveCoefficient;
}

/**
 * Enables or disables an effect
 */
void SerialFFB::setEffectState(uint8_t id, bool state){
	if(id >= effects.size()){
		return;
	}
	if(state){
		effects[id].startTime = HAL_GetTick() + effects[id].startDelay;
	}
	effects[id].state = state ? 1 : 0;
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
		if(cmd.type == CMDtype::setat){
			setMagnitude(cmd.adr, cmd.val);
		}else if(cmd.type == CMDtype::getat && cmd.adr < effects.size()){
			replies.emplace_back(effects[cmd.adr].magnitude);
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxstate:
		if(cmd.adr < effects.size()){
			if(cmd.type == CMDtype::setat){
				setEffectState(cmd.adr,cmd.val);
			}else if(cmd.type == CMDtype::getat){
				replies.emplace_back(effects[cmd.adr].state);
			}else{
				return CommandStatus::ERR;
			}
		}else{
			return CommandStatus::ERR;
		}

		break;

	case SerialEffects_commands::fxperiod:
		if(cmd.adr < effects.size())
			return handleGetSet(cmd, replies, effects[cmd.adr].period);
		else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxduration:
		if(cmd.adr < effects.size())
			return handleGetSet(cmd, replies, effects[cmd.adr].duration);
		else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxoffset:
		if(cmd.adr < effects.size()){ // Set both condition offset and periodic offset
			handleGetSet(cmd, replies, effects[cmd.adr].conditions[getCommandHandlerInstance()].cpOffset);
			return handleGetSet(cmd, replies, effects[cmd.adr].offset);
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxdeadzone:
		if(cmd.adr < effects.size())
			return handleGetSet(cmd, replies, effects[cmd.adr].conditions[getCommandHandlerInstance()].deadBand);
		else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxaxisgain:
		if(cmd.adr < effects.size()){
			if(cmd.type == CMDtype::getat){ // Must rescale between 0/1 float to uint16
				replies.emplace_back(effects[cmd.adr].axisMagnitudes[getCommandHandlerInstance()] * 0xffff);
			}else if(cmd.type == CMDtype::setat){
				effects[cmd.adr].axisMagnitudes[getCommandHandlerInstance()] = (float)cmd.val / 0xffff;
			}
		}
		else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxsat:
		if(cmd.adr < effects.size()){
			handleGetSet(cmd, replies, effects[cmd.adr].conditions[getCommandHandlerInstance()].negativeSaturation);
			return handleGetSet(cmd, replies, effects[cmd.adr].conditions[getCommandHandlerInstance()].positiveSaturation);
		}else
			return CommandStatus::ERR;
		break;
	case SerialEffects_commands::fxtype:
		if(cmd.adr < effects.size() && cmd.type == CMDtype::getat){
			replies.emplace_back(effects[cmd.adr].type);
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxcoeff:
		if(cmd.adr < effects.size()){
			handleGetSet(cmd, replies, effects[cmd.adr].conditions[getCommandHandlerInstance()].negativeCoefficient);
			return handleGetSet(cmd, replies, effects[cmd.adr].conditions[getCommandHandlerInstance()].positiveCoefficient);
		}else
			return CommandStatus::ERR;
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return status;
}
