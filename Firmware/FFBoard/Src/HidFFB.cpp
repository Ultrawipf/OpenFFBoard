/*
 * HidFFB.cpp
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#include "HidFFB.h"
#include "math.h"
#include "flash_helpers.h"
#include "usbd_customhid.h"
#include "cppmain.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

HidFFB::HidFFB() {
	this->registerHidCallback();
}

HidFFB::~HidFFB() {

}

bool HidFFB::getFfbActive(){
	return this->ffb_active;
}

void HidFFB::saveFlash(){
	uint16_t effects1 = this->getIdleSpringStrength() | (this->getFrictionStrength() << 8);
	Flash_Write(ADR_FFB_EFFECTS1, effects1);
	Flash_Write(ADR_FFB_EFFECTS2, this->cfFilter_f);
}
void HidFFB::restoreFlash(){
	uint16_t effects1 = 0;
	if(Flash_Read(ADR_FFB_EFFECTS1, &effects1)){
		this->setFrictionStrength((effects1 >> 8) & 0xff);
		this->setIdleSpringStrength(effects1 & 0xff);
	}
	effects1 = 0;
	if(Flash_Read(ADR_FFB_EFFECTS2, &effects1)){
		setCfFilter(effects1);
	}
}

uint8_t HidFFB::HID_SendReport(uint8_t *report,uint16_t len){
	return USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report, len);
}

/*
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

/*
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

/*
 * Called when HID OUT data is received via USB
 */
void HidFFB::hidOut(uint8_t* report){
	hid_out_period = (micros() - lastOut); // For measuring update rate
	lastOut = micros();
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
		sendStatusReport(0);
		break;
	case HID_ID_GAINREP: // Set global gain
		gain = report[1];
		break;
	case HID_ID_ENVREP: // TODO Envelope
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
	case HID_ID_CSTMREP: // Custom. pretty much never used
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
			//printf("Stop %d\n",report[1]);
		}else{
			if(effects[id].state != 1){
				set_filters(&effects[id]);
				effects[id].counter = 0; // When an effect was stopped reset all parameters that could cause jerking
			}
			//printf("Start %d\n",report[1]);
			effects[id].state = 1; //Start
		}
		sendStatusReport(report[1]);
		break;
	}
	case HID_ID_BLKFRREP: // Free a block
	{
		free_effect(report[1]-1);
		break;
	}

	default:
		printf("Got unknown HID command: %d",event_idx);
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

/*
 * Called on HID feature GET events
 * Any reply is assigned to the return_buf
 *
 * Handles block load reports and pool status which are requested after a new effect has been created
 */
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
	default:
		printf("Unknown get\n");
		break;
	}
}

void HidFFB::start_FFB(){
	ffb_active = true;
}
void HidFFB::stop_FFB(){
	ffb_active = false;
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
	//printf("Creating Effect: %d at %d\n",effect->effectType,index);

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
		effect_p->counter = 0;
		effect_p->last_value = 0;
		set_filters(effect_p);
	}

	effect_p->duration = effect->duration;
	//printf("SetEffect: %d, Axis: %d,Type: %d\n",effect->effectBlockIndex,effect->enableAxis,effect->effectType);
	if(!ffb_active)
		start_FFB();
	sendStatusReport(effect->effectBlockIndex);
}

void HidFFB::setCfFilter(uint32_t freq){

	cfFilter_f = clip<uint32_t,uint32_t>(freq,1,calcfrequency/2);
	float f = (float)cfFilter_f/(float)calcfrequency;
	for(FFB_Effect effect : effects){
		if(effect.type == FFB_EFFECT_CONSTANT){
			effect.filter->setFc(f);
		}
	}
}

float HidFFB::getCfFilterFreq(){
	return cfFilter_f;
}

void HidFFB::set_filters(FFB_Effect* effect){
	switch(effect->type){
		case FFB_EFFECT_DAMPER:
			if(effect->filter != nullptr)
				effect->filter->setBiquad(BiquadType::lowpass,(float)damper_f/(float)calcfrequency, damper_q, (float)0.0);
			else
				effect->filter = new Biquad(BiquadType::lowpass,(float)damper_f/(float)calcfrequency, damper_q, (float)0.0);
			break;
		case FFB_EFFECT_FRICTION:
			if(effect->filter != nullptr)
				effect->filter->setBiquad(BiquadType::lowpass,(float)friction_f/(float)calcfrequency, friction_q, (float)0.0);
			else
				effect->filter = new Biquad(BiquadType::lowpass,(float)friction_f/(float)calcfrequency, friction_q, (float)0.0);
			break;
		case FFB_EFFECT_INERTIA:
			if(effect->filter != nullptr)
				effect->filter->setBiquad(BiquadType::lowpass,(float)inertia_f/(float)calcfrequency, inertia_q, (float)0.0);
			else
				effect->filter = new Biquad(BiquadType::lowpass,(float)inertia_f/(float)calcfrequency, inertia_q, (float)0.0);
			break;
		case FFB_EFFECT_CONSTANT:
			if(effect->filter != nullptr)
				effect->filter->setBiquad(BiquadType::lowpass,(float)cfFilter_f/(float)calcfrequency, cfFilter_q, (float)0.0);
			else
				effect->filter = new Biquad(BiquadType::lowpass,(float)cfFilter_f/(float)calcfrequency, cfFilter_q, (float)0.0);
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

/*
 * Set the strength of the spring effect if FFB is disabled
 */
void HidFFB::setIdleSpringStrength(uint8_t spring){
	this->idlespringstregth = spring;
	if(spring == 0){
		idlecenter = false;
	}else{
		idlecenter = true;
	}
}
uint8_t HidFFB::getIdleSpringStrength(){
	return this->idlespringstregth;
}
/*
 * Set the intensity of friction effects
 */
void HidFFB::setFrictionStrength(uint8_t strength){
	this->frictionscale = strength;
}
uint8_t HidFFB::getFrictionStrength(){
	return this->frictionscale;
}

/*
 * Calculates the resulting torque for FFB effects
 * Takes current position input scaled from -0x7fff to 0x7fff
 * Outputs a torque value from -0x7fff to 0x7fff (not yet clipped)
 */
int32_t HidFFB::calculateEffects(int32_t pos,uint8_t axis=1){
	if(!ffb_active){
		// Center when FFB is turned of with a spring effect
		if(idlecenter){
			int16_t idlespringclip = clip<int32_t,int32_t>((int32_t)idlespringstregth*50,0,10000);
			float idlespringscale = 0.5f + ((float)idlespringstregth * 0.01f);
			return clip<int32_t,int32_t>((int32_t)(-pos*idlespringscale),-idlespringclip,idlespringclip);
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
		{	// Constant force is just the force
			int32_t f = ((int32_t)effect->magnitude * (int32_t)(1+effect->gain)) / 256;
			// Optional filtering to reduce spikes
			if(cfFilter_f < calcfrequency/2){
				f = effect->filter->process(f);
			}
			result_torque -= f;
			break;
		}
		case FFB_EFFECT_SPRING:
		{
			float scale = 0.0004f; // Tune for desired strength

			int32_t force = 0;

			if(abs(pos-effect->offset) > effect->deadBand){
				if(pos < effect->offset){ // Deadband side
					force = clip<int32_t,int32_t>((effect->negativeCoefficient * scale * (pos - (effect->offset - effect->deadBand))),-effect->negativeSaturation,effect->positiveSaturation);
				}else{
					force = clip<int32_t,int32_t>((effect->positiveCoefficient * scale * (pos - (effect->offset + effect->deadBand))),-effect->negativeSaturation,effect->positiveSaturation);
				}
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
		case FFB_EFFECT_INERTIA:
		{
			// Acceleration
			break;
		}
		case FFB_EFFECT_FRICTION:
		case FFB_EFFECT_DAMPER:
		{

			int32_t force = 0;

			if(effect->counter == 0){
				effect->last_value = pos;
				break;
			}
			int32_t speed = pos - effect->last_value;
			effect->last_value = pos;

			float val = effect->filter->process(speed) * 0.0625f;

			// Only active outside deadband. Process filter always!
			if(abs(pos-effect->offset) < effect->deadBand){
				break;
			}
			// Calculate force
			force = clip<int32_t,int32_t>((int32_t)((effect->positiveCoefficient) * val),-effect->negativeSaturation,effect->positiveSaturation);
			force = (frictionscale * force) / 255;
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
