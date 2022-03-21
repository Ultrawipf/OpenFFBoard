/*
 * HidFFB.cpp
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#include <assert.h>
#include "HidFFB.h"
#include "flash_helpers.h"
#include "hid_device.h"
#include "cppmain.h"


HidFFB::HidFFB() {

	// Initialize reports
	blockLoad_report.effectBlockIndex = 1;
	blockLoad_report.ramPoolAvailable = MAX_EFFECTS-used_effects;
	blockLoad_report.loadStatus = 1;

	pool_report.ramPoolSize = MAX_EFFECTS;
	pool_report.maxSimultaneousEffects = MAX_EFFECTS;
	pool_report.memoryManagement = 1;


	this->registerHidCallback();
}

HidFFB::~HidFFB() {
}

void HidFFB::setEffectsCalculator(EffectsCalculator *ec) {
	this->effects_calc = ec;
	assert(effects_calc != nullptr);
	this->effects_calc->setEffectsArray(this->effects);
	this->effects_calc->setActive(this->ffb_active);
}


bool HidFFB::getFfbActive(){
	return this->ffb_active;
}

bool HidFFB::HID_SendReport(uint8_t *report,uint16_t len){
	return tud_hid_report(0, report, len); // ID 0 skips ID field
}

/**
 * Calculates the frequency of hid out reports
 */
uint32_t HidFFB::getRate(){

	if(micros() - lastOut > 1000000 || hid_out_period == 0){
		hid_out_period = 0;
		return 0;
	}else{
		return (1000000/hid_out_period);
	}
}

/**
 * Sends a status report for a specific effect
 */
void HidFFB::sendStatusReport(uint8_t effect){
	this->reportFFBStatus.effectBlockIndex = effect;
	this->reportFFBStatus.status = HID_ACTUATOR_POWER;
	if(this->ffb_active){
		this->reportFFBStatus.status |= HID_ENABLE_ACTUATORS;
		this->reportFFBStatus.status |= HID_EFFECT_PLAYING;
	}else{
		this->reportFFBStatus.status |= HID_EFFECT_PAUSE;
	}
	if(effect > 0 && effects[effect-1].state == 1)
		this->reportFFBStatus.status |= HID_EFFECT_PLAYING;
	//printf("Status: %d\n",reportFFBStatus.status);
	HID_SendReport(reinterpret_cast<uint8_t*>(&this->reportFFBStatus), sizeof(reportFFB_status_t));
}


/**
 * Called when HID OUT data is received via USB
 */
void HidFFB::hidOut(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize){
	hid_out_period = (micros() - lastOut); // For measuring update rate
	lastOut = micros();
	// FFB Output Message
	const uint8_t* report = buffer;
	uint8_t event_idx = buffer[0] - FFB_ID_OFFSET;

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
		sendStatusReport(0);
		break;
	case HID_ID_GAINREP: // Set global gain
		set_gain(report[1]);
		break;
	case HID_ID_ENVREP: // Envelope
		set_envelope((FFB_SetEnvelope_Data_t *)report);
		break;
	case HID_ID_CONDREP: // Spring, Damper, Friction, Inertia
		set_condition((FFB_SetCondition_Data_t*)report);
		break;
	case HID_ID_PRIDREP: // Periodic
		set_periodic((FFB_SetPeriodic_Data_t*)report);
		break;
	case HID_ID_CONSTREP: // Constant
		set_constant_effect((FFB_SetConstantForce_Data_t*)report);
		break;
	case HID_ID_RAMPREP: // Ramp
		set_ramp((FFB_SetRamp_Data_t *)report);
		break;
	case HID_ID_CSTMREP: // Custom. pretty much never used
		//printf("Customrep");
		break;
	case HID_ID_SMPLREP: // Download sample
		//printf("Sampledl");
		break;
	case HID_ID_EFOPREP: //Effect operation
	{
		// Start or stop effect
		uint8_t id = report[1]-1;
		if(report[2] == 3){
			effects[id].state = 0; //Stop
								   //printf("Stop %d\n",report[1]);
		}else{
			// 1 = start, 2 = start solo
			if(report[2] == 2){
				for(FFB_Effect& effect : effects){
					effect.state = 0; // Stop all other effects
				}
			}
			if(effects[id].state != 1){
				set_filters(&effects[id]);
				//effects[id].startTime = 0; // When an effect was stopped reset all parameters that could cause jerking
			}

			//printf("Start %d\n",report[1]);
			effects[id].startTime = HAL_GetTick() + effects[id].startDelay; // + effects[id].startDelay;
			effects[id].state = 1; //Start


		}
		//sendStatusReport(report[1]);
		break;
	}
	case HID_ID_BLKFRREP: // Free a block
	{
		free_effect(report[1]-1);
		break;
	}

	default:
		break;
	}

}

void HidFFB::free_effect(uint16_t idx){
	if(idx < MAX_EFFECTS){
		effects[idx].type=FFB_EFFECT_NONE;
		for(int i=0; i< MAX_AXIS; i++) {
			if(effects[idx].filter[i] != nullptr){
				effects[idx].filter[i].reset(nullptr);
			}
		}
	}
}

/**
 * Called on HID feature GET events
 * Any reply is assigned to the return buffer
 *
 * Handles block load reports and pool status which are requested after a new effect has been created
 */
uint16_t HidFFB::hidGet(uint8_t report_id, hid_report_type_t report_type,uint8_t* buffer, uint16_t reqlen){
	// Feature gets go here

	uint8_t id = report_id - FFB_ID_OFFSET;

	switch(id){
	case HID_ID_BLKLDREP:
		//printf("Get Block Report\n");
		// Notice: first byte ID is not present in the reply buffer because it is handled by tinyusb internally!
		memcpy(buffer,&this->blockLoad_report,sizeof(FFB_BlockLoad_Feature_Data_t));
		return sizeof(FFB_BlockLoad_Feature_Data_t);
		break;
	case HID_ID_POOLREP:
		//printf("Get Pool Report\n");
		memcpy(buffer,&this->pool_report,sizeof(FFB_PIDPool_Feature_Data_t));
		return sizeof(FFB_PIDPool_Feature_Data_t);
		break;
	default:
		break;
	}
	return 0;
}

void HidFFB::start_FFB(){
	this->set_FFB(true);
}

void HidFFB::stop_FFB(){
	this->set_FFB(false);
}

void HidFFB::set_FFB(bool state)
{
	assert(effects_calc != nullptr);
	this->ffb_active = state;
	effects_calc->setActive(state);
}

void HidFFB::set_gain(uint8_t gain){
	assert(effects_calc != nullptr);
	effects_calc->setGain(gain);
}

void HidFFB::set_filters(FFB_Effect *effect){
	assert(effects_calc != nullptr);
	effects_calc->setFilters(effect);
}

void HidFFB::ffb_control(uint8_t cmd){
	//printf("Got Control signal: %d\n",cmd);
	if(cmd & 0x01){ //enable
		start_FFB();
	}if(cmd & 0x02){ //disable
		stop_FFB();
	}if(cmd & 0x04){ //stop TODO Some games send wrong commands?
		stop_FFB();
		//start_FFB();
	}if(cmd & 0x08){ //reset
		//ffb_active = true;
		stop_FFB();
		reset_ffb();
		// reset effects
	}if(cmd & 0x10){ //pause
		stop_FFB();
	}if(cmd & 0x20){ //continue
		start_FFB();
	}
}


void HidFFB::set_constant_effect(FFB_SetConstantForce_Data_t* data){
	FFB_Effect& effect_p = effects[data->effectBlockIndex-1];

	effect_p.magnitude = data->magnitude;
	if(effect_p.state == 0){
		effect_p.state = 1; // Force start effect
	}
}

void HidFFB::new_effect(FFB_CreateNewEffect_Feature_Data_t* effect){
	// Allocates a new effect

	uint8_t index = find_free_effect(effect->effectType); // next effect
	if(index == 0){
		blockLoad_report.loadStatus = 2;
		return;
	}
	//CommandHandler::logSerial("Creating Effect: " + std::to_string(effect->effectType) +  " at " + std::to_string(index) + "\n");
	FFB_Effect new_effect;
	new_effect.type = effect->effectType;
	this->effects_calc->logEffectType(effect->effectType);

	set_filters(&new_effect);

	effects[index-1] = std::move(new_effect);
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

	if (effect_p->type != effect->effectType){
		effect_p->startTime = 0;
		set_filters(effect_p);
	}

	effect_p->gain = effect->gain;
	effect_p->type = effect->effectType;
	effect_p->samplePeriod = effect->samplePeriod;

	effect_p->enableAxis = effect->enableAxis;
	effect_p->directionX = effect->directionX;
	effect_p->directionY = effect->directionY;
#if MAX_AXIS == 3
	effect_p->directionZ = effect->directionZ;
#endif
	if(effect_p->duration == 0){ // Fix for games assuming 0 is infinite
		effect_p->duration = FFB_EFFECT_DURATION_INFINITE;
	}
	effect_p->duration = effect->duration;
	effect_p->startDelay = effect->startDelay;
	if(!ffb_active)
		start_FFB();
	//sendStatusReport(effect->effectBlockIndex); // TODO required?
}

void HidFFB::set_condition(FFB_SetCondition_Data_t *cond){
	uint8_t axis = cond->parameterBlockOffset;
	if (axis >= MAX_AXIS){
		return; // sanity check!
	}
	FFB_Effect *effect = &effects[cond->effectBlockIndex - 1];
	effect->conditions[axis].cpOffset = cond->cpOffset;
	effect->conditions[axis].negativeCoefficient = cond->negativeCoefficient;
	effect->conditions[axis].positiveCoefficient = cond->positiveCoefficient;
	effect->conditions[axis].negativeSaturation = cond->negativeSaturation;
	effect->conditions[axis].positiveSaturation = cond->positiveSaturation;
	effect->conditions[axis].deadBand = cond->deadBand;
	//effect->conditionsCount++;
	if(effect->conditions[axis].positiveSaturation == 0){
		effect->conditions[axis].positiveSaturation = 0x7FFF;
	}
	if(effect->conditions[axis].negativeSaturation == 0){
		effect->conditions[axis].negativeSaturation = 0x7FFF;
	}
}

void HidFFB::set_envelope(FFB_SetEnvelope_Data_t *report){
	FFB_Effect *effect = &effects[report->effectBlockIndex - 1];

	effect->attackLevel = report->attackLevel;
	effect->attackTime = report->attackTime;
	effect->fadeLevel = report->fadeLevel;
	effect->fadeTime = report->fadeTime;
	effect->useEnvelope = true;
}
void HidFFB::set_ramp(FFB_SetRamp_Data_t *report){
	FFB_Effect *effect = &effects[report->effectBlockIndex - 1];
	effect->magnitude = 0x7fff; // Full magnitude for envelope calculation. This effect does not have a periodic report
	effect->startLevel = report->startLevel;
	effect->endLevel = report->endLevel;
}

void HidFFB::set_periodic(FFB_SetPeriodic_Data_t* report){
	FFB_Effect* effect = &effects[report->effectBlockIndex-1];

	effect->period = clip<uint32_t,uint32_t>(report->period,1,0x7fff); // Period is never 0
	effect->magnitude = report->magnitude;
	effect->offset = report->offset;
	effect->phase = report->phase;
	//effect->counter = 0;
}

uint8_t HidFFB::find_free_effect(uint8_t type){ //Will return the first effect index which is empty or the same type
	for(uint8_t i=0;i<MAX_EFFECTS;i++){
		if(effects[i].type == FFB_EFFECT_NONE){
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
