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

	setCanFilter();

	if(port->getSpeedPreset() < 3){
		port->setSpeedPreset(3); // Minimum 250k
	}

	this->port->setSilentMode(false);
	this->registerCommands();
	this->port->takePort();
	this->Start();
}

ODriveCAN::~ODriveCAN() {
	this->setTorque(0.0);
	this->port->removeCanFilter(filterId);
	this->port->freePort();
}

void ODriveCAN::setCanFilter(){
	// Set up a filter to receive odrive commands
	uint32_t filter_id = (nodeId & 0x3F) << 5;
	uint32_t filter_mask = 0x07E0;

	CAN_filter filterConf;
	filterConf.buffer = motorId % 2 == 0 ? 0 : 1;
	filterConf.filter_id = filter_id;
	filterConf.filter_mask =  filter_mask;
	this->filterId = this->port->addCanFilter(filterConf);
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
	registerCommand("storepos", ODriveCAN_commands::storepos, "Store encoder offset",CMDFLAG_GET | CMDFLAG_SET);
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

	}

	uint16_t settings1 = 0;
	if(Flash_Read(setting1addr, &settings1)){
		maxTorque = (float)clip(settings1 & 0xfff, 0, 0xfff) / 100.0;
		uint8_t settings1_2 = (settings1 >> 12) & 0xf;
		reloadPosAfterStartup = (settings1_2 & 0x1) != 0;
	}

	if(reloadPosAfterStartup){
		int16_t posOfs = 0;
		if(Flash_Read(motorId == 0 ? ADR_ODRIVE_OFS_M0 : ADR_ODRIVE_OFS_M1, (uint16_t*)&posOfs)){
			posOffset = (float)posOfs / getCpr();
		}
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

	Flash_Write(ADR_ODRIVE_CANID,canIds);

	uint16_t settings1 = ((int32_t)(maxTorque*100) & 0xfff);
	uint8_t settings1_2 = reloadPosAfterStartup ? 1 : 0; // 4 bits
	settings1 |= (settings1_2 & 0xf) << 12;

	Flash_Write(setting1addr, settings1);

	if(reloadPosAfterStartup){
		int32_t posOfs = posOffset * getCpr();
		Flash_Write(motorId == 0 ? ADR_ODRIVE_OFS_M0 : ADR_ODRIVE_OFS_M1, posOfs);
	}
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
				if(!reloadPosAfterStartup){
					this->setPos(0); // Assume this as the zero position and let the user correct it
				}
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
	msg.header.rtr = true;
	msg.header.length = 0;
	msg.header.id = cmd | (nodeId << 5);
	port->sendMessage(msg);
}

float ODriveCAN::getPos_f(){
	// Do not start a new request if already waiting and within timeout
	if(this->connected && (!posWaiting || HAL_GetTick() - lastPosTime > 5)){
		posWaiting = true;
		requestMsg(0x09);
	}
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
	//if(motorReady()){
		sendMsg<float>(0x0E,torque);
	//}
}

EncoderType ODriveCAN::getEncoderType(){
	return EncoderType::absolute;
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
			replies.emplace_back(lastVoltage*1000);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case ODriveCAN_commands::errors:
		if(cmd.type == CMDtype::get){
			replies.emplace_back((uint32_t)errors);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case ODriveCAN_commands::canid:
		handleGetSet(cmd, replies, this->nodeId);
		port->removeCanFilter(filterId);
		setCanFilter();
		break;
	case ODriveCAN_commands::state:
		if(cmd.type == CMDtype::get){
			replies.emplace_back((uint32_t)odriveCurrentState);
		}else{
			return CommandStatus::ERR;
		}
		break;
	case ODriveCAN_commands::canspd:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(port->getSpeedPreset());
		}else if(cmd.type == CMDtype::set){
			port->setSpeedPreset(std::max<uint8_t>(3,cmd.val));
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
			replies.emplace_back(val);
		}else if(cmd.type == CMDtype::set){
			maxTorque = (float)clip(cmd.val, 0, 0xfff) / 100.0;
		}else{
			return CommandStatus::ERR;
		}
		break;
	}
	case ODriveCAN_commands::connected:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(connected ? 1 : 0);
		}
		break;
	case ODriveCAN_commands::storepos:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(reloadPosAfterStartup ? 1 : 0);
		}else if(cmd.type == CMDtype::set){
			reloadPosAfterStartup = cmd.val != 0;
		}
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;

}

void ODriveCAN::canErrorCallback(CANPort* port,uint32_t errcode){
	//pulseErrLed();
}

void ODriveCAN::canRxPendCallback(CANPort* port,CAN_rx_msg& msg){
	uint16_t node = (msg.header.id >> 5) & 0x3F;
	if(node != this->nodeId){
		return;
	}
	uint64_t msg_int = *reinterpret_cast<uint64_t*>(msg.data);
	uint8_t cmd = msg.header.id & 0x1F;

	lastCanMessage = HAL_GetTick();

	switch(cmd){
	case 1:
	{
		// TODO error handling
		errors = (msg_int & 0xffffffff);
		odriveCurrentState = (ODriveState)( (msg_int >> 32) & 0xff);
		odriveMotorFlags = (msg_int >> 40) & 0xff;
		odriveEncoderFlags = ((msg_int >> 48) & 0xff);
		odriveControllerFlags = (msg_int >> 56) & 0xff;

		if(waitReady){
			waitReady = false;
			state = ODriveLocalState::WAIT_READY;
		}
		break;
	}
	case 0x09: // encoder pos float
	{
		uint64_t tp = msg_int & 0xffffffff;
		memcpy(&lastPos,&tp,sizeof(float));

		uint64_t ts = (msg_int >> 32) & 0xffffffff;
		memcpy(&lastSpeed,&ts,sizeof(float));
		lastPosTime = HAL_GetTick();
		posWaiting = false;

		break;
	}

	case 0x17: // voltage
	{
		lastVoltageUpdate = HAL_GetTick();
		uint64_t t = msg_int & 0xffffffff;
		memcpy(&lastVoltage,&t,sizeof(float));

		break;
	}



	default:
	break;
	}

}

#endif
