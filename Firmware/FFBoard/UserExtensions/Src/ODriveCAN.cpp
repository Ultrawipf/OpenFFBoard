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
		 .id=CLSID_MOT_ODRV0,	// 5
};
bool ODriveCAN2::inUse = false;
ClassIdentifier ODriveCAN2::info = {
		 .name = "ODrive (M1)" ,
		 .id=CLSID_MOT_ODRV1,	// 6
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

ODriveCAN::ODriveCAN(uint8_t id)  : CommandHandler("odrv", CLSID_MOT_ODRV0,id),  Thread("ODRIVE", ODRIVE_THREAD_MEM, ODRIVE_THREAD_PRIO), motorId(id) {

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
	this->port->setSilentMode(false);
	this->registerCommands();
	this->port->start();
	this->Start();
}

ODriveCAN::~ODriveCAN() {
	this->setTorque(0.0);
	this->port->removeCanFilter(filterId);
}

void ODriveCAN::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("canid", ODriveCAN_commands::canid, "CAN id of ODrive",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("canspd", ODriveCAN_commands::canspd, "CAN baudrate",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("errors", ODriveCAN_commands::errors, "ODrive error flags",CMDFLAG_GET);
	registerCommand("state", ODriveCAN_commands::state, "ODrive state",CMDFLAG_GET);
	registerCommand("maxtorque", ODriveCAN_commands::maxtorque, "Max torque to send for scaling",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("vbus", ODriveCAN_commands::vbus, "ODrive Vbus",CMDFLAG_GET);
	registerCommand("anticogging", ODriveCAN_commands::anticogging, "Set 1 to start anticogging calibration",CMDFLAG_SET);
	registerCommand("connected", ODriveCAN_commands::connected, "ODrive connection state",CMDFLAG_GET);
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

		if(HAL_GetTick() - lastCanMessage > 2000){ // Timeout
			odriveCurrentState = ODriveState::AXIS_STATE_UNDEFINED;
			state = ODriveLocalState::IDLE;
			waitReady = true;
			connected = false;
		}else{
			connected = true;
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
	if(this->connected)
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


CommandStatus ODriveCAN::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<ODriveCAN_commands>(cmd.cmdId)){
	case ODriveCAN_commands::vbus:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(lastVoltage*1000));
		}else{
			return CommandStatus::ERR;
		}
		break;

	case ODriveCAN_commands::errors:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply((uint32_t)errors));
		}else{
			return CommandStatus::ERR;
		}
		break;

	case ODriveCAN_commands::canid:
		return handleGetSet(cmd, replies, this->nodeId);
		break;
	case ODriveCAN_commands::state:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply((uint32_t)odriveCurrentState));
		}else{
			return CommandStatus::ERR;
		}
		break;
	case ODriveCAN_commands::canspd:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(baudrate));
		}else if(cmd.type == CMDtype::set){
			setCanRate(cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;
	case ODriveCAN_commands::anticogging:
		if(cmd.type == CMDtype::set && cmd.val == 1){
			this->startAnticogging();
		}else{
			return CommandStatus::ERR;
		}
		break;
	case ODriveCAN_commands::maxtorque:
	{
		if(cmd.type == CMDtype::get){
			int32_t val = maxTorque*100;
			replies.push_back(CommandReply(val));
		}else if(cmd.type == CMDtype::set){
			maxTorque = (float)clip(cmd.val, 0, 0xfff) / 100.0;
		}else{
			return CommandStatus::ERR;
		}
		break;
	}
	case ODriveCAN_commands::connected:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(connected ? 1 : 0));
		}
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;

}


void ODriveCAN::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){

	uint16_t node = (rxHeader->StdId >> 5) & 0x3F;
	if(node != this->nodeId){
		return;
	}
	uint64_t msg = *reinterpret_cast<uint64_t*>(rxBuf);
	uint8_t cmd = rxHeader->StdId & 0x1F;

	lastCanMessage = HAL_GetTick();

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
