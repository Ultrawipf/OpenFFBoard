/*
 * NormalizedAxis.h
 *
 *  Created on: 21.01.2021
 *      Author: Lidgard
 */

#include "NormalizedAxis.h"

ClassIdentifier NormalizedAxis::info = {
	.name = "Axis",
	.id = 1,
	.unique = 'X',
	.hidden = false};
//
//const ClassIdentifier NormalizedAxis::getInfo()
//{
//	return info;
//}


NormalizedAxis::NormalizedAxis(char axis) {
	this->axis = axis; // Axis are indexed from 1-X to 3-Z

	if(axis == 'X'){
		flashAddrs = NormalizedAxisFlashAddrs_t({ADR_AXIS1_ENDSTOP, ADR_AXIS1_POWER, ADR_AXIS1_DEGREES});
	}else if(axis == 'Y'){
		flashAddrs = NormalizedAxisFlashAddrs_t({ADR_AXIS2_ENDSTOP, ADR_AXIS2_POWER, ADR_AXIS2_DEGREES});
	}else if(axis == 'Z'){
		flashAddrs = NormalizedAxisFlashAddrs_t({ADR_AXIS3_ENDSTOP, ADR_AXIS3_POWER, ADR_AXIS3_DEGREES});
	}

//	restoreFlash();
}

NormalizedAxis::~NormalizedAxis() {
//
}

const ClassIdentifier NormalizedAxis::getInfo() {
	return ClassIdentifier {NormalizedAxis::info.name, NormalizedAxis::info.id, axis, NormalizedAxis::info.hidden};
}

metric_t* NormalizedAxis::getMetrics() {
	return &metric.current;
}

int32_t NormalizedAxis::getLastScaledEnc() {
	return  clip(metric.current.pos,-0x7fff,0x7fff);
}


/*
 * Read parameters from flash and restore settings
 */
void NormalizedAxis::restoreFlash(){
	uint16_t esval, power;
	if(Flash_Read(flashAddrs.endstop, &esval)) {
		fx_ratio_i = esval & 0xff;
		endstop_gain = (esval >> 8) & 0xff;
	}


	Flash_Read(flashAddrs.power, &power);
	setPower(power);
	Flash_Read(flashAddrs.degrees, &degreesOfRotation);
	nextDegreesOfRotation = degreesOfRotation;
}


// Saves parameters to flash
void NormalizedAxis::saveFlash(){
	Flash_Write(flashAddrs.endstop, fx_ratio_i | (endstop_gain << 8));

	Flash_Write(flashAddrs.power, power);
	Flash_Write(flashAddrs.degrees, degreesOfRotation);
}


void NormalizedAxis::updateIdleSpringForce(float idlespringscale, int16_t idlespringclip) {
	effectTorque = clip<int32_t,int32_t>((int32_t)(-metric.current.pos*idlespringscale),-idlespringclip,idlespringclip);
}

void NormalizedAxis::setFxRatio(uint8_t val) {
	fx_ratio_i = val;
	updateTorqueScaler();
}


void NormalizedAxis::resetMetrics(int32_t new_pos= 0) { // pos is scaledEnc
	metric.current.pos = new_pos;
	metric.previous.pos = metric.current.pos;
	metric.current.speed = 0;
	metric.current.accel = 0;
	metric.previous.speed = 0;
	metric.previous.torque = 0;
	metric.current.torque = 0;
}


void NormalizedAxis::updateMetrics(int32_t new_pos) { // pos is scaledEnc
	metric.current.pos = new_pos;
	metric.current.speed = metric.current.pos - metric.previous.pos;
	metric.current.accel = metric.current.speed - metric.previous.speed;
	metric.previous.pos = metric.current.pos;
	metric.previous.speed = metric.current.speed;
	metric.previous.torque = metric.current.torque;
	metric.current.torque = 0;
}

void NormalizedAxis::setPower(uint16_t val){
	power = val;
	updateTorqueScaler();
}

uint16_t NormalizedAxis::getPower(){
	return power;
}

void  NormalizedAxis::updateTorqueScaler() {
	float effect_margin_scaler = ((float)fx_ratio_i/255.0);
	torqueScaler = ((float)power / (float)0x7fff) * effect_margin_scaler;
}

float NormalizedAxis::getTorqueScaler(){
	return torqueScaler;
}


int32_t NormalizedAxis::getTorque() { return metric.current.torque; }

bool NormalizedAxis::isInverted() {
	return invertAxis;
}

/*
 * Calculate soft endstop effect
 */
int16_t NormalizedAxis::updateEndstop(){
	int8_t clipdir = cliptest<int32_t,int32_t>(metric.current.pos, -0x7fff, 0x7fff);
	if(clipdir == 0){
		return 0;
	}
	int32_t addtorque = clip<int32_t,int32_t>(abs(metric.current.pos)-0x7fff,-0x7fff,0x7fff);
	addtorque *= (float)endstop_gain * 0.2f; // Apply endstop gain for stiffness
	addtorque *= -clipdir;

	return clip<int32_t,int32_t>(addtorque,-0x7fff,0x7fff);
}

void NormalizedAxis::setEffectTorque(int32_t torque) {
	effectTorque= torque;
}

// pass in ptr to receive the sum of the effects + endstop torque
// return true if torque is clipping
bool NormalizedAxis::updateTorque(int32_t* totalTorque) {

	if(abs(effectTorque) >= 0x7fff){
		pulseClipLed();
	}

	// Scale effect torque
	// float effect_margin_scaler = ((float)fx_ratio_i/255.0);
	// effectTorque *= ((float)power / (float)0x7fff) * effect_margin_scaler;
	effectTorque  *= torqueScaler;

	int32_t torque = effectTorque + updateEndstop();
	
	torque = (invertAxis) ? -torque : torque;
	metric.current.torque = torque;

	// Torque changed
	bool torqueChanged = torque != metric.previous.torque;
	if(torqueChanged) {
		torque = clip<int32_t, int32_t>(torque, -power, power);
		if (abs(torque) == power){
			pulseClipLed();
		}
	}
	*totalTorque = torque;
	return (torqueChanged);
}




void NormalizedAxis::processHidCommand(HID_Custom_Data_t* data) {

	switch (data->cmd&0x3F){
		case HID_CMD_FFB_ESGAIN:
			if(data->type == HidCmdType::write) {
				endstop_gain = data->data;
			}
			if(data->type == HidCmdType::request){
				data->data = endstop_gain;
			}
		break;

		case HID_CMD_FFB_FXRATIO:
			if(data->type == HidCmdType::write) {
				setFxRatio(data->data);
			}
			else if(data->type == HidCmdType::request){
				data->data = fx_ratio_i;
			}
		break;

		case HID_CMD_FFB_DEGREES:
			if (data->type == HidCmdType::write){
				nextDegreesOfRotation = data->data;
			}
			else if (data->type == HidCmdType::request){
				data->data = degreesOfRotation;
			}
		break;
		
		default:

		break;
		}
	return;
}


ParseStatus NormalizedAxis::command(ParsedCommand *cmd, std::string *reply)
{
	ParseStatus flag = ParseStatus::OK;
	// ------------ General commands ----------------

	if (cmd->cmd == "fxratio")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(fx_ratio_i);
		}
		else if (cmd->type == CMDtype::set)
		{
			setFxRatio(cmd->val);
		}
	}
	else if (cmd->cmd == "power")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(power);
		}
		else if (cmd->type == CMDtype::set)
		{
			setPower(cmd->val);
		}
	}
	else if (cmd->cmd == "degrees")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(degreesOfRotation);
		}
		else if (cmd->type == CMDtype::set)
		{
			nextDegreesOfRotation = cmd->val;
		}
	}	
	else if (cmd->cmd == "esgain")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(endstop_gain);
		}
		else if (cmd->type == CMDtype::set)
		{
			endstop_gain = cmd->val;
		}
	}
	else if (cmd->cmd == "invert")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += invertAxis ? "1" : "0";
		}
		else if (cmd->type == CMDtype::set)
		{
			invertAxis = cmd->val >= 1 ? true : false;
		}
	}else{
		flag = ParseStatus::NOT_FOUND;
	}

	return flag;
};

