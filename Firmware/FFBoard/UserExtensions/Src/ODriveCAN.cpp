/*
 * OdriveCAN.cpp
 *
 *  Created on: Jun 21, 2021
 *      Author: Yannick
 */
#include "target_constants.h"
#ifdef ODRIVE
#include <ODriveCAN.h>

bool ODriveCAN1::inUse = false;
ClassIdentifier ODriveCAN1::info = {
		 .name = "ODrive (M0)" ,
		 .id=5,
		 .unique = '0'
};
bool ODriveCAN2::inUse = false;
ClassIdentifier ODriveCAN2::info = {
		 .name = "ODrive (M1)" ,
		 .id=6,
		 .unique = '1'
};



const ClassIdentifier ODriveCAN1::getInfo(){
	return info;
}

const ClassIdentifier ODriveCAN2::getInfo(){
	return info;
}

bool ODriveCAN1::isCreatable(){
	return !ODriveCAN1::inUse; // Creatable if not already in use for example by another axis
}

bool ODriveCAN2::isCreatable(){
	return !ODriveCAN2::inUse; // Creatable if not already in use for example by another axis
}

ODriveCAN::ODriveCAN(uint8_t id)  : Thread("ODRIVE", ODRIVE_THREAD_MEM, ODRIVE_THREAD_PRIO), motorId(id) {

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

ODriveCAN::~ODriveCAN() {
	this->setTorque(0.0);
}

void ODriveCAN::restoreFlash(){
	uint16_t setting1addr = ADR_ODRIVE_SETTING1_M0;

	uint16_t canIds = 0x3040;
	if(Flash_Read(ADR_ODRIVE_CANID, &canIds)){
		if(motorId == 0){
			nodeId = canIds & 0x3f;
		}else if(motorId == 1){
			nodeId = (canIds >> 6) & 0x3f;
			setting1addr = ADR_ODRIVE_SETTING1_M1;
		}
		uint8_t canspd = (canIds >> 12) & 0x7;
		this->setCanRate(canspd);
	}

	uint16_t settings1 = 0;
	if(Flash_Read(setting1addr, &settings1)){
		maxTorque = (float)clip(settings1 & 0xfff, 0, 0xfff) / 100.0;
	}
}

void ODriveCAN::saveFlash(){
	uint16_t setting1addr = ADR_ODRIVE_SETTING1_M0;

	uint16_t canIds = 0x3040;
	Flash_Read(ADR_ODRIVE_CANID, &canIds); // Read again
	if(motorId == 0){
		canIds &= ~0x3F; // reset bits
		canIds |= nodeId & 0x3f;
	}else if(motorId == 1){
		setting1addr = ADR_ODRIVE_SETTING1_M1;
		canIds &= ~0xFC0; // reset bits
		canIds |= (nodeId & 0x3f) << 6;
	}
	canIds &= ~0x7000; // reset bits
	canIds |= (this->baudrate & 0x7) << 12;
	Flash_Write(ADR_ODRIVE_CANID,canIds);

	uint16_t settings1 = ((int32_t)(maxTorque*100) & 0xfff);
	Flash_Write(setting1addr, settings1);
}

void ODriveCAN::Run(){
	while(true){
		this->Delay(500);

		switch(state){
		case ODriveLocalState::WAIT_READY:
			if(this->odriveCurrentState == ODriveState::AXIS_STATE_IDLE){
				this->state = ODriveLocalState::WAIT_CALIBRATION; // Wait
				this->setState(ODriveState::AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
			}

		break;

		// Calibration in progress. wait until its finished to enter torque mode
		case ODriveLocalState::WAIT_CALIBRATION_DONE:
			if(odriveCurrentState == ODriveState::AXIS_STATE_IDLE){
				setState(ODriveState::AXIS_STATE_CLOSED_LOOP_CONTROL);
				state = ODriveLocalState::START_RUNNING;

			}else if(odriveCurrentState == ODriveState::AXIS_STATE_CLOSED_LOOP_CONTROL){
				state = ODriveLocalState::START_RUNNING;
			}
		break;

		case ODriveLocalState::START_RUNNING:
			state = ODriveLocalState::RUNNING;
			// Odrive is active,
			// enable torque mode
			if(odriveCurrentState == ODriveState::AXIS_STATE_CLOSED_LOOP_CONTROL){
				this->setPos(0);
				setMode(ODriveControlMode::CONTROL_MODE_TORQUE_CONTROL, ODriveInputMode::INPUT_MODE_PASSTHROUGH);
			}

		break;
		default:

		break;
		}

		// If odrive is currently performing any calibration task wait until finished
		if(odriveCurrentState == ODriveState::AXIS_STATE_FULL_CALIBRATION_SEQUENCE || odriveCurrentState == ODriveState::AXIS_STATE_MOTOR_CALIBRATION || odriveCurrentState == ODriveState::AXIS_STATE_ENCODER_OFFSET_CALIBRATION || odriveCurrentState == ODriveState::AXIS_STATE_ENCODER_HALL_PHASE_CALIBRATION || odriveCurrentState == ODriveState::AXIS_STATE_ENCODER_INDEX_SEARCH){
			if(state != ODriveLocalState::WAIT_CALIBRATION_DONE)
				state = ODriveLocalState::WAIT_CALIBRATION_DONE;
		// If closed loop mode is on assume its ready and already calibrated
		}else if(odriveCurrentState == ODriveState::AXIS_STATE_CLOSED_LOOP_CONTROL){
			if(state != ODriveLocalState::START_RUNNING && state != ODriveLocalState::RUNNING)
				state = ODriveLocalState::RUNNING;
		}

		if(HAL_GetTick() - lastVoltageUpdate > 1000){
			requestMsg(0x17); // Update voltage
		}
	}
}

void ODriveCAN::stopMotor(){
	active = false;
	this->setTorque(0.0);
	if(odriveCurrentState == ODriveState::AXIS_STATE_CLOSED_LOOP_CONTROL)
		this->setState(ODriveState::AXIS_STATE_IDLE);
}

void ODriveCAN::startMotor(){
	active = true;
	if(odriveCurrentState == ODriveState::AXIS_STATE_IDLE)
		this->setState(ODriveState::AXIS_STATE_CLOSED_LOOP_CONTROL);
}

Encoder* ODriveCAN::getEncoder(){
	return static_cast<Encoder*>(this);
}

/**
 * Must be in encoder cpr if not just used to zero the axis
 */
void ODriveCAN::setPos(int32_t pos){
	// Only change encoder count internally as offset
	posOffset = lastPos - ((float)pos / (float)getCpr());
}


void ODriveCAN::requestMsg(uint8_t cmd){
	CAN_tx_msg msg;

	msg.header.RTR = CAN_RTR_REMOTE;
	msg.header.DLC = 0;
	msg.header.StdId = cmd | (nodeId << 5);
	port->sendMessage(msg);
}

float ODriveCAN::getPos_f(){
	if(motorReady())
		requestMsg(0x09);
	return lastPos-posOffset;
}

bool ODriveCAN::motorReady(){
	return state == ODriveLocalState::RUNNING && (odriveCurrentState == ODriveState::AXIS_STATE_CLOSED_LOOP_CONTROL);
}
int32_t ODriveCAN::getPos(){
	return getCpr() * getPos_f();
}


uint32_t ODriveCAN::getCpr(){
	return 0xffff;
}
void ODriveCAN::setTorque(float torque){
	if(motorReady())
		sendMsg<float>(0x0E,torque);
}

void ODriveCAN::setMode(ODriveControlMode controlMode,ODriveInputMode inputMode){
	uint64_t mode = (uint64_t) controlMode | ((uint64_t)inputMode << 32);
	sendMsg(0x0B,mode);
}

void ODriveCAN::setState(ODriveState state){
	sendMsg(0x07,(uint32_t)state);
}


void ODriveCAN::turn(int16_t power){
	float torque = ((float)power / (float)0x7fff) * maxTorque;
	this->setTorque(torque);
}

void ODriveCAN::setCanRate(uint8_t canRate){
	baudrate = clip<uint8_t,uint8_t>(canRate, 3, 5);
	port->setSpeedPreset(baudrate);
}

/**
 * Sends the start anticogging command
 */
void ODriveCAN::startAnticogging(){
	sendMsg(0x10, (uint8_t)0);
}


ParseStatus ODriveCAN::command(ParsedCommand* cmd,std::string* reply){
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
			maxTorque = (float)clip(cmd->val, 0, 0xfff) / 100.0;
		}
	}else if(cmd->cmd == "odriveVbus"){
		if(cmd->type == CMDtype::get){
			//requestMsg(0x17); // Update voltage for next time
			int32_t val = lastVoltage*1000;
			*reply += std::to_string(val);
		}

	}else if(cmd->cmd == "odriveErrors"){
		if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint32_t)errors);
		}

	}else if(cmd->cmd == "odriveAnticogging"){
		if(cmd->type == CMDtype::set && cmd->val == 1){
			this->startAnticogging();
		}else{
			*reply+="=1 to start calibration sequence";
		}
	}else if(cmd->cmd == "odriveState"){
		if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint32_t)odriveCurrentState);
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


void ODriveCAN::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){

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
		errors = (msg & 0xffffffff);
		odriveCurrentState = (ODriveState)( (msg >> 32) & 0xff);
		odriveMotorFlags = (msg >> 40) & 0xff;
		odriveEncoderFlags = ((msg >> 48) & 0xff);
		odriveControllerFlags = (msg >> 56) & 0xff;

		if(waitReady){
			waitReady = false;
			state = ODriveLocalState::WAIT_READY;
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
