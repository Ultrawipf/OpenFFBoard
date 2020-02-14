/*
 * HidFFB.cpp
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#include "HidFFB.h"
#include "math.h"

HidFFB::HidFFB() {
	this->registerHidCallback();
}

HidFFB::~HidFFB() {

}


void HidFFB::hidOut(uint8_t* report){
	// FFB Output Message
	report[0] -= FFB_ID_OFFSET;// if offset id was set correct this
	uint8_t event_idx = report[0];


	// -------- Out Reports --------
	switch(event_idx){
	case HID_ID_NEWEFREP: //add Effect Report. Feature
		new_effect((FFB_CreateNewEffect_Feature_Data_t*)(report));
		break;
	case HID_ID_EFFREP: // Set Effect
		set_effect((FFB_SetEffect_t*)(report));
		break;
	case HID_ID_CTRLREP: // Control report. 1=Enable Actuators, 2=Disable Actuators, 4=Stop All Effects, 8=Reset, 16=Pause, 32=Continue
		ffb_control(report[1]);
		break;
	case HID_ID_GAINREP:
		gain = report[1];
		break;
	case HID_ID_ENVREP:
		printf("Envrep");
		break;
	case HID_ID_CONDREP:
		//FFB_SetCondition_Data_t
		set_condition((FFB_SetCondition_Data_t*)report);
		break;
	case HID_ID_PRIDREP: // Periodic
		set_periodic((FFB_SetPeriodic_Data_t*)report);
		break;
	case HID_ID_CONSTREP: // Constant
		set_constant_effect((FFB_SetConstantForce_Data_t*)report);
		break;
	case HID_ID_RAMPREP: // Ramp
		printf("Ramprep");
		break;
	case HID_ID_CSTMREP: // Custom. TODO
		printf("Customrep");
		break;
	case HID_ID_SMPLREP: // Download sample
		printf("Sampledl");
		break;
	case HID_ID_EFOPREP: //Effect operation
	{
		// Start or stop effect
		uint8_t id = report[1]-1;
		if(report[2] == 3){
			effects[id].state = 0; //Stop
		}else{
			effects[id].state = 1; //Start
		}
		break;
	}
	case HID_ID_BLKFRREP: // Free a block
	{
		free_effect(report[1]-1);
		break;
	}

	default:
		printf("Got unknown command: %d",event_idx);
		break;
	}

}

void HidFFB::free_effect(uint16_t idx){
	if(idx < MAX_EFFECTS){
		effects[idx].type=FFB_EFFECT_NONE;
		if(effects[idx].filter != nullptr){
			delete effects[idx].filter;
			effects[idx].filter = nullptr;
		}
	}
}


void HidFFB::hidGet(uint8_t id,uint16_t len,uint8_t** return_buf){
	// Feature gets go here

	id = id - FFB_ID_OFFSET;

	switch(id){
	case HID_ID_BLKLDREP:
		//printf("Get Block Report\n");
		*return_buf = (uint8_t*)(&this->blockLoad_report);
		break;
	case HID_ID_POOLREP:
		//printf("Get Pool Report\n");
		*return_buf = (uint8_t*)(&this->pool_report);
		break;
	}
}

void HidFFB::start_FFB(){
	ffb_active = true;


}
void HidFFB::stop_FFB(){
	ffb_active = false;


	//TODO Callbacks?
}

void HidFFB::ffb_control(uint8_t cmd){
	printf("Got Control signal: %d\n",cmd);
	if(cmd & 0x01){ //enable
		start_FFB();
	}if(cmd & 0x02){ //disable
		ffb_active = false;
	}if(cmd & 0x04){ //stop TODO Some games send wrong commands?
		stop_FFB();
		//start_FFB();
	}if(cmd & 0x08){ //reset
		//ffb_active = true;
		stop_FFB();
		reset_ffb();
		// reset effects
	}if(cmd & 0x10){ //pause
		ffb_active = false;
	}if(cmd & 0x20){ //continue
		ffb_active = true;
	}
	Bchg(this->reportFFBStatus.status,HID_ENABLE_ACTUATORS & ffb_active);
}


void HidFFB::set_constant_effect(FFB_SetConstantForce_Data_t* effect){
	effects[effect->effectBlockIndex-1].magnitude = effect->magnitude;
}

void HidFFB::new_effect(FFB_CreateNewEffect_Feature_Data_t* effect){
	// Allocates a new effect

	uint8_t index = find_free_effect(effect->effectType); // next effect
	if(index == 0){
		blockLoad_report.loadStatus = 2;
		return;
	}
	printf("Creating Effect: %d with size %d at %d\n",effect->effectType,effect->byteCount,index);

	FFB_Effect new_effect;
	new_effect.type = effect->effectType;

	set_filters(&new_effect);

	effects[index-1] = new_effect;
	// Set block load report
	reportFFBStatus.effectBlockIndex = index;
	blockLoad_report.effectBlockIndex = index;
	used_effects++;
	blockLoad_report.ramPoolAvailable = MAX_EFFECTS-used_effects;
	blockLoad_report.loadStatus = 1;


}
void HidFFB::set_effect(FFB_SetEffect_t* effect){
	uint8_t index = effect->effectBlockIndex;
	if(index > MAX_EFFECTS || index == 0)
		return;

	FFB_Effect* effect_p = &effects[index-1];
	effect_p->gain = effect->gain;
	effect_p->type = effect->effectType;
	effect_p->samplePeriod = effect->samplePeriod;
	if(effect->enableAxis & 0x4){
		// All axes
		effect_p->axis = 0x7;
	}else{
		effect_p->axis = effect->enableAxis;
	}
	if(effect_p->type != effect->effectType){
		set_filters(effect_p);
	}

	effect_p->duration = effect->duration;
	//printf("SetEffect: %d, Axis: %d,Type: %d\n",effect->effectType,effect->enableAxis,effect->effectType);
	if(!ffb_active)
		start_FFB();
}

void HidFFB::set_filters(FFB_Effect* effect){
	switch(effect->type){
		case FFB_EFFECT_DAMPER:
			if(effect->filter != nullptr)
				effect->filter->setBiquad(damper_type,(float)damper_f/frequency, damper_q, (float)0.0);
			else
				effect->filter = new Biquad(damper_type,(float)damper_f/frequency, damper_q, (float)0.0);
			break;
		case FFB_EFFECT_FRICTION:
			if(effect->filter != nullptr)
				effect->filter->setBiquad(friction_type,(float)friction_f/frequency, friction_q, (float)0.0);
			else
				effect->filter = new Biquad(friction_type,(float)friction_f/frequency, friction_q, (float)0.0);
			break;
	}
}

void HidFFB::set_condition(FFB_SetCondition_Data_t* cond){
	if(cond->parameterBlockOffset != 0) //TODO if more axes are needed. Only X Axis is implemented now for the wheel.
		return;

	FFB_Effect* effect = &effects[cond->effectBlockIndex-1];

	effect->offset = cond->cpOffset;
	effect->negativeCoefficient = cond->negativeCoefficient;
	effect->positiveCoefficient = cond->positiveCoefficient;
	effect->negativeSaturation = cond->negativeSaturation;
	effect->positiveSaturation = cond->positiveSaturation;
	effect->deadBand = cond->deadBand;



}

void HidFFB::set_periodic(FFB_SetPeriodic_Data_t* report){
	FFB_Effect* effect = &effects[report->effectBlockIndex-1];

	effect->period = report->period;
	effect->magnitude = report->magnitude;
	effect->offset = report->offset;
	effect->phase = report->phase;
	effect->counter = 0;
}

uint8_t HidFFB::find_free_effect(uint8_t type){ //Will return the first effect index which is empty or the same type
	for(uint8_t i=0;i<MAX_EFFECTS;i++){
		if(effects[i].type == FFB_EFFECT_NONE || effects[i].type == type){
			return(i+1);
		}
	}
	return 0;
}



void HidFFB::reset_ffb(){
	for(uint8_t i=0;i<MAX_EFFECTS;i++){
		free_effect(i);
	}
	this->reportFFBStatus.effectBlockIndex = 1;
	this->reportFFBStatus.status = (HID_ACTUATOR_POWER) | (HID_ENABLE_ACTUATORS);
	used_effects = 0;
}

int32_t HidFFB::calculateEffects(int32_t pos,uint8_t axis=1){
	if(!ffb_active){ // Don't calculate effects if clipping occured
		// TODO weak center spring? when FFB off?
		if(idlecenter){
			return clip<int32_t,int32_t>(-pos,-5000,5000);
		}else{
			return 0;
		}
	}

	int32_t result_torque = 0;

	for(uint8_t i = 0;i<MAX_EFFECTS;i++){
		FFB_Effect* effect = &effects[i];
		// Filter out inactive effects
		if(effect->state == 0 || !(axis & effect->axis))
			continue;

		switch(effect->type){
		case FFB_EFFECT_CONSTANT:
			result_torque -= (effect->magnitude * (1+effect->gain)) >> 8;
			break;

		case FFB_EFFECT_SPRING:
		{
			int32_t force = 0;
			if(pos<effect->offset){
				force = clip<int32_t,int32_t>(((int32_t)(effect->negativeCoefficient>>3) * (pos - (effect->offset))) >> 6,-effect->negativeSaturation,effect->positiveSaturation);
			}else{
				force = clip<int32_t,int32_t>(((int32_t)(effect->positiveCoefficient>>3) * (pos - (effect->offset))) >> 6,-effect->negativeSaturation,effect->positiveSaturation);
			}
			result_torque -= force;
			break;
		}

		case FFB_EFFECT_SQUARE:
		{

			int32_t force =  ((effect->counter + effect->phase) % ((uint32_t)effect->period+2)) < (uint32_t)(effect->period+2)/2 ? -effect->magnitude : effect->magnitude;
			force += effect->offset;
			result_torque -= force;
			break;
		}
		case FFB_EFFECT_SINE:
		{
			uint16_t t = effect->counter;
			float freq = 1.0f/(float)(std::max<uint16_t>(effect->period,2));
			float phase = (float)effect->phase/(float)35999; //degrees
			float sine =  sinf(2.0*(float)M_PI*(t*freq+phase)) * effect->magnitude;
			int32_t force = (int32_t)(effect->offset + sine);

			result_torque -= force;
			break;
		}
		case FFB_EFFECT_FRICTION:
		case FFB_EFFECT_DAMPER:
		{
			int32_t force = 0;
			int32_t speed = pos - effect->last_value;
			effect->last_value = pos;
			float val = effect->filter->process(speed);
			force = clip<int32_t,int32_t>((effect->positiveCoefficient>>3) * val,-effect->negativeSaturation,effect->positiveSaturation);

			result_torque -= force;
			break;
		}
		default:
			// Unsupported effect
			break;
		}

		if(effect->counter++ > effect->duration){
			effect->state = 0;
		}

	}
	result_torque = (result_torque * (gain+1)) >> 8; // Apply global gain

	return result_torque; //clip(result_torque,-0x7fff,0x7fff);
}
