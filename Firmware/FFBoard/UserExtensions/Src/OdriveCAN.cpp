/*
 * OdriveCAN.cpp
 *
 *  Created on: Jun 21, 2021
 *      Author: Yannick
 */
#include "target_constants.h"
#ifdef ODRIVE
#include "OdriveCAN.h"

bool OdriveCAN1::inUse = false;
ClassIdentifier OdriveCAN1::info = {
		 .name = "Odrive (M0)" ,
		 .id=5,
		 .unique = '0'
};
bool OdriveCAN2::inUse = false;
ClassIdentifier OdriveCAN2::info = {
		 .name = "Odrive (M1)" ,
		 .id=6,
		 .unique = '1'
};



const ClassIdentifier OdriveCAN1::getInfo(){
	return info;
}

const ClassIdentifier OdriveCAN2::getInfo(){
	return info;
}

bool OdriveCAN1::isCreatable(){
	return !OdriveCAN1::inUse; // Creatable if not already in use for example by another axis
}

bool OdriveCAN2::isCreatable(){
	return !OdriveCAN2::inUse; // Creatable if not already in use for example by another axis
}

OdriveCAN::OdriveCAN(uint8_t id)  : Thread("ODRIVE", ODRIVE_THREAD_MEM, ODRIVE_THREAD_PRIO), motorId(id) {

	if(motorId == 0){
		nodeId = 0;
	}else if(motorId == 1){
		nodeId = 1; // defaults
	}
	restoreFlash();

	// Set up a filter to receive odrive commands
	CAN_FilterTypeDef sFilterConfig;
	sFilterConfig.FilterBank = 0;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = 0x0000;
	sFilterConfig.FilterIdLow = 0x0000;
	sFilterConfig.FilterMaskIdHigh = 0x0000;
	sFilterConfig.FilterMaskIdLow = 0x0000;
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.SlaveStartFilterBank = 14;
	this->filterId = this->port->addCanFilter(sFilterConfig);

	this->port->setSpeedPreset(baudrate);

	this->Start();
}

OdriveCAN::~OdriveCAN() {
	this->setTorque(0.0);
}

void OdriveCAN::restoreFlash(){
	uint16_t canIds = 0x3040;
	if(Flash_Read(ADR_ODRIVE_CANID, &canIds)){
		if(motorId == 0){
			nodeId = canIds & 0x3f;
		}else if(motorId == 1){
			nodeId = (canIds >> 6) & 0x3f;
		}
		uint8_t canspd = canIds >> 12 & 0x7;
		this->setCanRate(canspd);
	}

	uint16_t settings1 = 0;
	if(Flash_Read(ADR_ODRIVE_SETTING1, &settings1)){
		maxTorque = (float)clip(settings1 & 0xfff, 0, 100) / 100.0;
	}
}

void OdriveCAN::saveFlash(){
	uint16_t canIds = 0x3040;
	Flash_Read(ADR_ODRIVE_CANID, &canIds); // Read again
	if(motorId == 0){
		canIds &= ~0x3F; // reset bits
		canIds |= nodeId & 0x3f;
	}else if(motorId == 1){
		canIds &= ~0xFC0; // reset bits
		nodeId = (canIds & 0x3f) << 6;
	}
	canIds &= ~0x7000; // reset bits
	canIds |= (this->baudrate & 0x7) << 12;
	Flash_Write(ADR_ODRIVE_CANID,canIds);

	uint16_t settings1 = ((int32_t)(maxTorque*100) & 0xfff);
	Flash_Write(ADR_ODRIVE_SETTING1, settings1);
}

void OdriveCAN::Run(){
	while(true){
		this->Delay(500);
		if(requestFirstRun){
			requestFirstRun = false;
			readyCb();
		}
//		else if(active && ready){
//
//			if(currentState == OdriveState::AXIS_STATE_IDLE){
//				this->setState(OdriveState::AXIS_STATE_CLOSED_LOOP_CONTROL);
//			}
//
//			}
		if(HAL_GetTick() - lastVoltageUpdate > 1000){
			requestMsg(0x17); // Update voltage
		}
	}
}

void OdriveCAN::stopMotor(){
	active = false;
	this->setTorque(0.0);
	if(currentState == OdriveState::AXIS_STATE_CLOSED_LOOP_CONTROL)
		this->setState(OdriveState::AXIS_STATE_IDLE);
}

void OdriveCAN::startMotor(){
	active = true;
	if(currentState == OdriveState::AXIS_STATE_IDLE)
		this->setState(OdriveState::AXIS_STATE_CLOSED_LOOP_CONTROL);
}

Encoder* OdriveCAN::getEncoder(){
	return static_cast<Encoder*>(this);
}

/**
 * Must be in encoder cpr if not just used to zero the axis
 */
void OdriveCAN::setPos(int32_t pos){
	// Only change encoder count internally as offset
	posOffset = lastPos - ((float)pos / (float)getCpr());
}

void OdriveCAN::sendMsg(uint8_t cmd,uint32_t value){
	CAN_tx_msg msg;

	memcpy(&msg.data,&value,4);
	msg.header.RTR = CAN_RTR_DATA;
	msg.header.DLC = 4;
	msg.header.StdId = cmd | (nodeId << 5);
	port->sendMessage(msg);
}

void OdriveCAN::sendMsg(uint8_t cmd,uint64_t value){
	CAN_tx_msg msg;
	memcpy(&msg.data,&value,8);
	msg.header.RTR = CAN_RTR_DATA;
	msg.header.DLC = 8;
	msg.header.StdId = cmd | (nodeId << 5);
	port->sendMessage(msg);
}

void OdriveCAN::sendMsg(uint8_t cmd,float value){
	CAN_tx_msg msg;

	memcpy(&msg.data,&value,4);
	msg.header.RTR = CAN_RTR_DATA;
	msg.header.DLC = 4;
	msg.header.StdId = cmd | (nodeId << 5);
	port->sendMessage(msg);
}

void OdriveCAN::requestMsg(uint8_t cmd){
	CAN_tx_msg msg;

	msg.header.RTR = CAN_RTR_REMOTE;
	msg.header.DLC = 0;
	msg.header.StdId = cmd | (nodeId << 5);
	port->sendMessage(msg);
}

float OdriveCAN::getPos_f(){
	if(motorReady())
		requestMsg(0x09);
	return lastPos-posOffset;
}

bool OdriveCAN::motorReady(){
	return ready && (currentState == OdriveState::AXIS_STATE_CLOSED_LOOP_CONTROL);
}
int32_t OdriveCAN::getPos(){
	return getCpr() * getPos_f();
}


uint32_t OdriveCAN::getCpr(){
	return 0xffff;
}
void OdriveCAN::setTorque(float torque){
	if(motorReady())
		sendMsg(0x0E,torque);
}

void OdriveCAN::setMode(OdriveControlMode controlMode,OdriveInputMode inputMode){
	uint64_t mode = (uint64_t) controlMode | ((uint64_t)inputMode << 32);
	sendMsg(0x0B,mode);
}

void OdriveCAN::setState(OdriveState state){
	sendMsg(0x07,(uint32_t)state);
}


void OdriveCAN::turn(int16_t power){
	float torque = ((float)power / (float)0x7fff) * maxTorque;
	this->setTorque(torque);
}

void OdriveCAN::setCanRate(uint8_t canRate){
	baudrate = clip<uint8_t,uint8_t>(canRate, 3, 5);
	port->setSpeedPreset(baudrate);
}

/**
 * Called after the first status message is received
 */
void OdriveCAN::readyCb(){
	ready = true; // waits for first reply
	this->setPos(0);
	setMode(OdriveControlMode::CONTROL_MODE_TORQUE_CONTROL, OdriveInputMode::INPUT_MODE_PASSTHROUGH);
	//this->setTorque(0.0);
	//this->setState(OdriveState::AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

}

ParseStatus OdriveCAN::command(ParsedCommand* cmd,std::string* reply){
	 // Prefix set but not our prefix
	if(cmd->prefix != this->getInfo().unique && cmd->prefix != '\0'){
		return ParseStatus::NOT_FOUND;
	}
	ParseStatus status = ParseStatus::OK;
	if(cmd->cmd == "odriveCanId"){
		handleGetSet(cmd, reply, this->nodeId);

	}else if(cmd->cmd == "odriveMaxTorque"){
		if(cmd->type == CMDtype::get){
			int32_t val = maxTorque*100;
			*reply += std::to_string(val);
		}else if(cmd->type == CMDtype::set){
			maxTorque = (float)clip(cmd->val, 0, 100) / 100.0;
		}
	}else if(cmd->cmd == "odriveVbus"){
		if(cmd->type == CMDtype::get){
			//requestMsg(0x17); // Update voltage for next time
			int32_t val = lastVoltage*1000;
			*reply += std::to_string(val);
		}

	}else if(cmd->cmd == "odriveErrors"){
		if(cmd->type == CMDtype::get){
			*reply += std::to_string(errors);
		}

	}else if(cmd->cmd == "odriveState"){
		if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint32_t)currentState);
		}

	}else if(cmd->cmd == "odriveCanSpd"){
		if(cmd->type == CMDtype::get){
			*reply += std::to_string(baudrate);
		}else if(cmd->type == CMDtype::set){
			setCanRate(cmd->val);
		}
	}else{
		status = ParseStatus::NOT_FOUND;
	}
	return status;
}


void OdriveCAN::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){

	uint16_t node = (rxHeader->StdId >> 5) & 0x3F;
	if(node != this->nodeId){
		return;
	}
	uint64_t msg = *reinterpret_cast<uint64_t*>(rxBuf);
	uint8_t cmd = rxHeader->StdId & 0x1F;

	switch(cmd){
	case 1:
	{
		// TODO error handling
		errors = msg & 0xffffffff;
		currentState = (OdriveState)( (msg >> 32) & 0xffffffff);
		if(waitReady){
			waitReady = false;
			requestFirstRun = true;
		}
		break;
	}
	case 0x09: // encoder pos float
	{
		uint64_t tp = msg & 0xffffffff;
		memcpy(&lastPos,&tp,sizeof(float));

		uint64_t ts = (msg >> 32) & 0xffffffff;
		memcpy(&lastSpeed,&ts,sizeof(float));
		break;
	}

	case 0x17: // voltage
	{
		lastVoltageUpdate = HAL_GetTick();
		uint64_t t = msg & 0xffffffff;
		memcpy(&lastVoltage,&t,sizeof(float));

		break;
	}



	default:
	break;
	}

}

#endif
