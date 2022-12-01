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

SerialFFB::SerialFFB(EffectsCalculator &ec) : CommandHandler("fxm", CLSID_EFFECTSMGR), effects_calc(ec), effects(ec.effects) {
	CommandHandler::registerCommands();
	//registerCommand("filterCfFreq", EffectsCalculator_commands::ffbfiltercf, "Constant force filter frequency", CMDFLAG_GET | CMDFLAG_SET);

	// First effect is preallocated CF
	effects[0].type = FFB_EFFECT_CONSTANT;
	effects[0].state = 0;

}

SerialFFB::~SerialFFB() {
	// TODO Auto-generated destructor stub
}


uint32_t SerialFFB::getRate(){

}
uint32_t SerialFFB::getConstantForceRate(){

}
bool SerialFFB::getFfbActive(){
	return this->ffb_active;
}

void SerialFFB::reset_ffb(){

}
void SerialFFB::set_FFB(bool state){
	this->ffb_active = state;
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


CommandStatus SerialFFB::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<SerialEffects_commands>(cmd.cmdId)){
	case SerialEffects_commands::ffbstate:
		break;
	case SerialEffects_commands::ffbreset:
		break;
	case SerialEffects_commands::newEffect:
		break;
	case SerialEffects_commands::fxmagnitude:
		break;
	}
}
