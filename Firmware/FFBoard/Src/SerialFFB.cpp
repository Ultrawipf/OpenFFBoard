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
		std::unique_ptr<Effect> new_effect;
		switch(effectType){
			case FFB_EFFECT_CONSTANT: new_effect = std::make_unique<EffectConstant>(); break;
			case FFB_EFFECT_RAMP: new_effect = std::make_unique<EffectRamp>(); break;
			case FFB_EFFECT_SQUARE: new_effect = std::make_unique<EffectSquare>(); break;
			case FFB_EFFECT_SINE: new_effect = std::make_unique<EffectSine>(); break;
			case FFB_EFFECT_TRIANGLE: new_effect = std::make_unique<EffectTriangle>(); break;
			case FFB_EFFECT_SAWTOOTHUP: new_effect = std::make_unique<EffectSawtoothUp>(); break;
			case FFB_EFFECT_SAWTOOTHDOWN: new_effect = std::make_unique<EffectSawtoothDown>(); break;
			case FFB_EFFECT_SPRING: new_effect = std::make_unique<EffectSpring>(); break;
			case FFB_EFFECT_DAMPER: new_effect = std::make_unique<EffectDamper>(); break;
			case FFB_EFFECT_INERTIA: new_effect = std::make_unique<EffectInertia>(); break;
			case FFB_EFFECT_FRICTION: new_effect = std::make_unique<EffectFriction>(); break;
			default: return -1;
		}
		new_effect->setDuration(FFB_EFFECT_DURATION_INFINITE);
		new_effect->setAxisMagnitude(std::min(this->getCommandHandlerInstance(),(uint8_t)MAX_AXIS), 1);
		new_effect->setUseSingleCondition(false);
		
		FFB_Effect_Condition cond;
		// default cond set zero
		cond.cpOffset = 0; cond.deadBand = 0; cond.negativeCoefficient = 0; cond.positiveCoefficient = 0; cond.negativeSaturation = 0; cond.positiveSaturation = 0;
		// Need proper setCondition but for simplicity we will just call it manually later
		this->effects_calc->logEffectType(effectType,false);
		effects[idx] = std::move(new_effect);
	}
	return idx;
}

/**
 * Changes magnitude of non conditional effects (Constant, ramp, square, triangle, sawtooth)
 */
void SerialFFB::setMagnitude(uint8_t idx,int16_t magnitude){
	if(idx > effects.size() || !effects[idx]){
		return;
	}
	auto& effect = effects[idx];
	effect->setMagnitude(magnitude);

	if(effect->getType() == FFB_EFFECT_CONSTANT){
		EffectsControlItf::cfUpdateEvent();
	}
}

/**
 * Enables or disables an effect
 */
void SerialFFB::setEffectState(uint8_t id, bool state){
	if(id >= effects.size() || !effects[id]){
		return;
	}
	if(state){
		effects[id]->setStartTime(HAL_GetTick() + effects[id]->getStartDelay());
	}
	effects[id]->setState(state ? 1 : 0);
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
		if(cmd.type == CMDtype::setat){
			setMagnitude(cmd.adr, cmd.val);
		}else if(cmd.type == CMDtype::getat && cmd.adr < effects.size() && effects[cmd.adr]){
			replies.emplace_back(effects[cmd.adr]->getMagnitude());
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxstate:
		if(cmd.adr < effects.size() && effects[cmd.adr]){
			if(cmd.type == CMDtype::setat){
				setEffectState(cmd.adr,cmd.val);
			}else if(cmd.type == CMDtype::getat){
				replies.emplace_back(effects[cmd.adr]->getState());
			}else{
				return CommandStatus::ERR;
			}
		}else{
			return CommandStatus::ERR;
		}

		break;

	case SerialEffects_commands::fxperiod:
		if(cmd.adr < effects.size() && effects[cmd.adr]) {
			if(cmd.type == CMDtype::setat) effects[cmd.adr]->setPeriod(cmd.val);
			else if(cmd.type == CMDtype::getat) replies.emplace_back(effects[cmd.adr]->getPeriod());
			return CommandStatus::OK;
		} else return CommandStatus::ERR;

	case SerialEffects_commands::fxduration:
		if(cmd.adr < effects.size() && effects[cmd.adr]) {
			if(cmd.type == CMDtype::setat) effects[cmd.adr]->setDuration(cmd.val);
			else if(cmd.type == CMDtype::getat) replies.emplace_back(effects[cmd.adr]->getDuration());
			return CommandStatus::OK;
		} else return CommandStatus::ERR;

	case SerialEffects_commands::fxoffset:
		if(cmd.adr < effects.size() && effects[cmd.adr]){ 
			if(cmd.type == CMDtype::setat) effects[cmd.adr]->setOffset(cmd.val);
			else if(cmd.type == CMDtype::getat) replies.emplace_back(effects[cmd.adr]->getOffset());
			return CommandStatus::OK;
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxdeadzone:
		if(cmd.adr < effects.size() && effects[cmd.adr]){
			FFB_Effect_Condition* cond = effects[cmd.adr]->getCondition(getCommandHandlerInstance());
			if (cond) {
				if(cmd.type == CMDtype::setat) cond->deadBand = cmd.val;
				else if(cmd.type == CMDtype::getat) replies.emplace_back(cond->deadBand);
			}
			return CommandStatus::OK;
		} else return CommandStatus::ERR;

	case SerialEffects_commands::fxaxisgain:
		if(cmd.adr < effects.size() && effects[cmd.adr]){
			if(cmd.type == CMDtype::getat){ 
				replies.emplace_back(effects[cmd.adr]->getAxisMagnitude(getCommandHandlerInstance()) * 0xffff);
			}else if(cmd.type == CMDtype::setat){
				effects[cmd.adr]->setAxisMagnitude(getCommandHandlerInstance(), (float)cmd.val / 0xffff);
			}
		}
		else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxsat:
		if(cmd.adr < effects.size() && effects[cmd.adr]){
			FFB_Effect_Condition* cond = effects[cmd.adr]->getCondition(getCommandHandlerInstance());
			if(cond){
				if(cmd.type == CMDtype::setat) { cond->negativeSaturation = cmd.val; cond->positiveSaturation = cmd.val; }
				else if(cmd.type == CMDtype::getat) replies.emplace_back(cond->positiveSaturation);
			}
			return CommandStatus::OK;
		}else return CommandStatus::ERR;

	case SerialEffects_commands::fxtype:
		if(cmd.adr < effects.size() && cmd.type == CMDtype::getat && effects[cmd.adr]){
			replies.emplace_back(effects[cmd.adr]->getType());
		}else
			return CommandStatus::ERR;
		break;

	case SerialEffects_commands::fxcoeff:
		if(cmd.adr < effects.size() && effects[cmd.adr]){
			FFB_Effect_Condition* cond = effects[cmd.adr]->getCondition(getCommandHandlerInstance());
			if (cond) {
				if(cmd.type == CMDtype::setat) { cond->negativeCoefficient = cmd.val; cond->positiveCoefficient = cmd.val; }
				else if(cmd.type == CMDtype::getat) replies.emplace_back(cond->positiveCoefficient);
			}
			return CommandStatus::OK;
		}else
			return CommandStatus::ERR;
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return status;
}
