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

SerialFFB::SerialFFB(EffectsCalculator &ec,uint8_t instance) : CommandHandler("fxm", CLSID_EFFECTSMGR,instance), effects_calc(ec), effects(ec.effects) {

	CommandHandler::registerCommands();
	registerCommand("ffbstate", SerialEffects_commands::ffbstate, "FFB active", CMDFLAG_GET);
	registerCommand("ffbreset", SerialEffects_commands::ffbreset, "Reset all effects or effect adr", CMDFLAG_GET | CMDFLAG_GETADR);
	registerCommand("new", SerialEffects_commands::newEffect, "Create new effect of type val. Returns index or -1 on err", CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("mag", SerialEffects_commands::fxmagnitude, "16b magnitude of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("state", SerialEffects_commands::fxstate, "Enable/Disable effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("period", SerialEffects_commands::fxperiod, "Period of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("duration", SerialEffects_commands::fxduration, "Duration of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("offset", SerialEffects_commands::fxoffset, "Offset of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("deadzone", SerialEffects_commands::fxdeadzone, "Deadzone of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("sat", SerialEffects_commands::fxsat, "Saturation of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	registerCommand("coeff", SerialEffects_commands::fxcoeff, "Coefficient of effect adr", CMDFLAG_SETADR | CMDFLAG_GETADR);
	// First effect is preallocated CF
	effects[0].type = FFB_EFFECT_CONSTANT;
	effects[0].state = 0;

}

SerialFFB::~SerialFFB() {
	// TODO Auto-generated destructor stub
}


uint32_t SerialFFB::getRate(){
	return 0;
}

uint32_t SerialFFB::getConstantForceRate(){
	return 0;
}

bool SerialFFB::getFfbActive(){
	return this->effects_calc.isActive();
}

void SerialFFB::reset_ffb(){
	for(uint8_t i=0;i<effects.size();i++){
		effects_calc.free_effect(i);
	}
}
void SerialFFB::set_FFB(bool state){
	this->effects_calc.setActive(state);
}
void SerialFFB::set_gain(uint8_t gain){
	effects_calc.setGain(gain); // Global gain
}

/**
 * Takes an effect type and allocates it in the effects array
 * Returns the index where the effect was created or -1 if it can not be created
 */
int32_t SerialFFB::newEffect(uint8_t effectType){
	uint32_t idx = this->effects_calc.find_free_effect(effectType);
	if(idx > 0){
		// Allocate effect
		effects[idx].type = effectType;
	}
	return idx;
}

void SerialFFB::setMagnitude(uint8_t idx,int16_t magnitude){
	if(idx > effects.size()){
		return;
	}
	FFB_Effect* effect = &effects[idx];
	effect->magnitude = magnitude;

	if(effect->type == FFB_EFFECT_CONSTANT){
		//Log CF rate
	}
}



CommandStatus SerialFFB::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	CommandStatus status = CommandStatus::OK;
	switch(static_cast<SerialEffects_commands>(cmd.cmdId)){
	case SerialEffects_commands::ffbstate:
		return handleGetFuncSetFunc(cmd, replies, &SerialFFB::getFfbActive, &SerialFFB::set_FFB, this);

	case SerialEffects_commands::ffbreset:
		if(cmd.type == CMDtype::get){
			reset_ffb();
		}else if(cmd.type == CMDtype::getat){
			effects_calc.free_effect(cmd.adr);
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::newEffect:
		if(cmd.type == CMDtype::set){
			replies.emplace_back(newEffect(cmd.val+1));
		}else if(cmd.type == CMDtype::info){
			replies.emplace_back("Constant=0,Ramp=1,Square=2,Sine=3,Triangle=4,Sawtooth Up=5,Sawtooth Down=6,Spring=7,Damper=8,Inertia=9,Friction=10");
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
		if(cmd.adr < effects.size())
			return handleGetSet(cmd, replies, effects[cmd.adr].state);
		else
			return CommandStatus::ERR;
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
			handleGetSet(cmd, replies, effects[cmd.adr].conditions[0].cpOffset);
			return handleGetSet(cmd, replies, effects[cmd.adr].offset);
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxdeadzone:
		if(cmd.adr < effects.size())
			return handleGetSet(cmd, replies, effects[cmd.adr].conditions[0].deadBand);
		else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxsat:
		if(cmd.adr < effects.size()){
			handleGetSet(cmd, replies, effects[cmd.adr].conditions[0].negativeSaturation);
			return handleGetSet(cmd, replies, effects[cmd.adr].conditions[0].positiveSaturation);
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxcoeff:
		if(cmd.adr < effects.size()){
			handleGetSet(cmd, replies, effects[cmd.adr].conditions[0].negativeCoefficient);
			return handleGetSet(cmd, replies, effects[cmd.adr].conditions[0].positiveCoefficient);
		}else
			return CommandStatus::ERR;
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return status;
}
