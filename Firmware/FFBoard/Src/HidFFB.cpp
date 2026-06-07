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
#include <math.h>

#ifdef USE_DSP_FUNCTIONS
#include "arm_math_types.h"
#include "dsp/fast_math_functions.h"
#define MATH_PI PI
#define MATH_SIN(x) arm_sin_f32(x)
#define MATH_COS(x) arm_cos_f32(x)
#else
#define MATH_PI M_PI
#define MATH_SIN(x) sinf(x)
#define MATH_COS(x) cosf(x)
#endif

HidFFB::HidFFB(std::shared_ptr<EffectsCalculator> ec,uint8_t axisCount) : effects_calc(ec), axisCount(axisCount)
{
	directionEnableMask = 1 << axisCount; // Direction enable bit is last bit after axis enable bits
	// Initialize reports
	blockLoad_report.effectBlockIndex = 1;
	blockLoad_report.ramPoolAvailable = (EffectsCalculator::max_effects-used_effects)*sizeof(EffectConstant);
	blockLoad_report.loadStatus = 1;

	pool_report.ramPoolSize = EffectsCalculator::max_effects*sizeof(EffectConstant);
	pool_report.maxSimultaneousEffects = EffectsCalculator::max_effects;
	pool_report.memoryManagement = 1;


	this->registerHidCallback();
}

HidFFB::~HidFFB() {
}


/**
 * Sets the mask where the direction enable bit is in the effect
 */
void HidFFB::setDirectionEnableMask(uint8_t mask){
	this->directionEnableMask = mask;
}

void HidFFB::updateSamplerate(float newSamplerate){
	effects_calc->updateSamplerate(newSamplerate);
}


bool HidFFB::getFfbActive(){
	return this->ffb_active;
}

bool HidFFB::HID_SendReport(uint8_t *report,uint16_t len){
	return tud_hid_report(0, report, len); // ID 0 skips ID field
}


/**
 * Sends a status report for a specific effect
 */
void HidFFB::sendStatusReport(uint8_t effect){
//	if(effect != 0){
//		this->reportFFBStatus.effectBlockIndex = effect;
//	}
	this->reportFFBStatus.status = HID_ACTUATOR_POWER;
	if(this->ffb_active){
		this->reportFFBStatus.status |= HID_ENABLE_ACTUATORS;
		this->reportFFBStatus.status |= HID_EFFECT_PLAYING;
	}else{
		this->reportFFBStatus.status |= HID_EFFECT_PAUSE;
	}
//	if(effect > 0 && effects_calc->getEffect(effect-1)->getState() == 1)
//		this->reportFFBStatus.status |= HID_EFFECT_PLAYING;
	//printf("Status: %d\n",reportFFBStatus.status);
	HID_SendReport(reinterpret_cast<uint8_t*>(&this->reportFFBStatus), sizeof(reportFFB_status_t));
}


/**
 * Called when HID OUT data is received via USB
 */
void HidFFB::hidOut(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize){
	fxUpdateEvent(); // use uint16_t for timer overflow handling if micros timer is used

	// FFB Output Message
	const uint8_t* report = buffer;
	uint8_t event_idx = report_id - FFB_ID_OFFSET;

	// -------- Out Reports --------
	switch(event_idx)
	{

		case HID_ID_NEWEFREP: //add Effect Report. Feature
			new_effect((FFB_CreateNewEffect_Feature_Data_t*)(report));
			break;
		case HID_ID_EFFREP: // Set Effect
		{
			FFB_SetEffect_t setEffectRepBuf;
			memcpy(&setEffectRepBuf,report,std::min<uint16_t>(sizeof(FFB_SetEffect_t),bufsize)); // Copy report to buffer. only valid range if less axes are used
			set_effect(&setEffectRepBuf);
			break;
		}
		case HID_ID_CTRLREP: // Control report. 1=Enable Actuators, 2=Disable Actuators, 4=Stop All Effects, 8=Reset, 16=Pause, 32=Continue
			ffb_control(report[1]);
			//sendStatusReport(0);
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
			set_effect_operation((FFB_EffOp_Data_t*)report);
			break;
		}
		case HID_ID_BLKFRREP: // Free a block
		{
			effects_calc->free_effect(report[1]-1);
			break;
		}

		default:
		{
			break;
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
#ifdef DEBUGLOG
	CommandHandler::logSerialDebug("FFB on");
#endif
	this->set_FFB(true);
}

void HidFFB::stop_FFB(){
#ifdef DEBUGLOG
	CommandHandler::logSerialDebug("FFB off");
#endif
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



void HidFFB::ffb_control(uint8_t cmd){

	if(cmd & 0x01){ //enable
		start_FFB();
	}if(cmd & 0x02){ //disable
		stop_FFB();
	}if(cmd & 0x04){ //stop
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
	if(data->effectBlockIndex == 0 || data->effectBlockIndex > EffectsCalculator::max_effects){
		return;
	}
	cfUpdateEvent();
	Effect* effect_p = effects_calc->getEffect(data->effectBlockIndex-1);
	if (effect_p) {
		effect_p->setConstantForce(data);
	}
}

void HidFFB::new_effect(FFB_CreateNewEffect_Feature_Data_t* effect){
	// Allocates a new effect

	int32_t index = effects_calc->find_free_effect(effect->effectType); // next effect
	if(index == -1){
		blockLoad_report.loadStatus = 2;
#ifdef DEBUGLOG
		CommandHandler::logSerialDebug("Can't allocate a new effect");
#endif
		return;
	}
	std::unique_ptr<Effect> new_effect;
	switch(effect->effectType){
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
		default: return; // Type non supporté
	}

	this->effects_calc->logEffectType(effect->effectType,false);
#ifdef DEBUGLOG
	CommandHandler::logSerialDebug("New effect type:" + std::to_string(effect->effectType) + " idx: " + std::to_string(index-1));
#endif

	effects_calc->setFilters(new_effect.get()); // Initialize filters correctly before assigning
	effects_calc->storeEffect(index, std::move(new_effect));
	
	// Set block load report
	blockLoad_report.effectBlockIndex = index+1;
	used_effects++;
	blockLoad_report.ramPoolAvailable = (EffectsCalculator::max_effects-used_effects)*sizeof(EffectConstant); // Approximate size
	blockLoad_report.loadStatus = 1;
	sendStatusReport(index+1);
}

/**
 * Sets up an effect
 * If the direction enable bit is set then only 1 condition block is used
 */
void HidFFB::set_effect(FFB_SetEffect_t* effect){
	uint8_t index = effect->effectBlockIndex;
	if(index > EffectsCalculator::max_effects || index == 0)
		return;

	Effect* effect_p = effects_calc->getEffect(index-1);
	if(!effect_p) return;

	if (effect_p->getType() != effect->effectType){
		// Unlikely but if type changes, we should recreate. Let's assume it doesn't.
		effect_p->setStartTime(0);
	}

	effect_p->setGain(effect->gain);

	bool directionEnable = (effect->enableAxis & this->directionEnableMask);
	bool overridesCondition = false;

	if(!effect_p->getUseSingleCondition() && (effect->effectType == FFB_EFFECT_SPRING || effect->effectType == FFB_EFFECT_DAMPER || effect->effectType == FFB_EFFECT_INERTIA || effect->effectType == FFB_EFFECT_FRICTION))
	{
		FFB_Effect_Condition* cond0 = effect_p->getCondition(0);
		FFB_Effect_Condition* cond1 = effect_p->getCondition(1);
		if(cond0 && cond0->isActive()){
			effect_p->setAxisMagnitude(0, 1.0f);
			overridesCondition = true;
		}
		if((cond1 && cond1->isActive()) || effect_p->getUseSingleCondition()){
			effect_p->setAxisMagnitude(1, 1.0f);
			overridesCondition = true;
		}
	}


	if(!overridesCondition){
		float phaseX = MATH_PI*2.0f * (effect->directionX/36000.0f);

		effect_p->setAxisMagnitude(0, directionEnable ? MATH_SIN(phaseX) : (effect->enableAxis & X_AXIS_ENABLE ? (effect->directionX - 18000.0f) / 18000.0f : 0)); 
		effect_p->setAxisMagnitude(1, directionEnable ? -MATH_COS(phaseX) : (effect->enableAxis & Y_AXIS_ENABLE ? -(effect->directionY - 18000.0f) / 18000.0f : 0));
	}

#if MAX_AXIS == 3
	float phaseY = MATH_PI*2.0f * (effect->directionY/36000.0f);
	effect_p->setAxisMagnitude(2, directionEnable ? MATH_SIN(phaseY) : (effect->enableAxis & Z_AXIS_ENABLE ? (effect->directionZ - 18000.0f) / 18000.0f : 0));
#endif
	if(effect->duration == 0){ 
		effect_p->setDuration(FFB_EFFECT_DURATION_INFINITE);
	}else{
		effect_p->setDuration(effect->duration);
	}
	effect_p->setStartDelay(effect->startDelay);
	if(!ffb_active)
		start_FFB();

	sendStatusReport(effect->effectBlockIndex);
}

/**
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
void HidFFB::set_condition(FFB_SetCondition_Data_t *cond){
	if(cond->effectBlockIndex == 0 || cond->effectBlockIndex > EffectsCalculator::max_effects){
		return;
	}
	
	Effect* effect = effects_calc->getEffect(cond->effectBlockIndex - 1);
	if (effect) {
		effect->setCondition(cond);
	}
}

void HidFFB::set_effect_operation(FFB_EffOp_Data_t* report){
	if(report->effectBlockIndex == 0 || report->effectBlockIndex > EffectsCalculator::max_effects){
		return; // Invalid ID
	}
	// Start or stop effect
	uint8_t id = report->effectBlockIndex-1;
	Effect* effect = effects_calc->getEffect(id);
	if(!effect) return;

	if(report->state == 3){
		effect->setState(0); //Stop
	}else{

		// 1 = start, 2 = start solo
		if(report->state == 2){
			for(uint32_t i = 0; i < EffectsCalculator::max_effects; i++){
				Effect* eff = effects_calc->getEffect(i);
				if(eff) eff->setState(0); // Stop all other effects
			}
		}
		
		effect->setStartTime(HAL_GetTick() + effect->getStartDelay());
		effect->setState(1); //Start
	}
	
	this->effects_calc->logEffectState(effect->getType(), effect->getState());
}


void HidFFB::set_envelope(FFB_SetEnvelope_Data_t *report){
	if(report->effectBlockIndex == 0 || report->effectBlockIndex > EffectsCalculator::max_effects){
		return;
	}
	Effect* effect = effects_calc->getEffect(report->effectBlockIndex - 1);
	if(effect) effect->setEnvelope(report);
}

void HidFFB::set_ramp(FFB_SetRamp_Data_t *report){
	if(report->effectBlockIndex == 0 || report->effectBlockIndex > EffectsCalculator::max_effects){
		return;
	}
	Effect* effect = effects_calc->getEffect(report->effectBlockIndex - 1);
	if(effect) effect->setRamp(report);
}

void HidFFB::set_periodic(FFB_SetPeriodic_Data_t* report){
	if(report->effectBlockIndex == 0 || report->effectBlockIndex > EffectsCalculator::max_effects){
		return;
	}
	Effect* effect = effects_calc->getEffect(report->effectBlockIndex-1);
	if(effect) effect->setPeriodic(report);
}


void HidFFB::reset_ffb(){
	for(uint8_t i=0;i<EffectsCalculator::max_effects;i++){
		effects_calc->free_effect(i);
	}
	//this->reportFFBStatus.effectBlockIndex = 1;
	this->reportFFBStatus.status = (HID_ACTUATOR_POWER) | (HID_ENABLE_ACTUATORS);
	used_effects = 1;
}
