/*
 * OdriveCAN.cpp
 *
 *  Created on: Jun 21, 2021
 *      Author: Yannick
 */
#include "target_constants.h"
#ifdef ODRIVE
#include "OdriveCAN.h"

bool OdriveCAN::inUse = false;

ClassIdentifier OdriveCAN::info = {
		 .name = "Odrive" ,
		 .id=5,
		 .unique = '0'
 };
const ClassIdentifier OdriveCAN::getInfo(){
	return info;
}

bool OdriveCAN::isCreatable(){
	return !OdriveCAN::inUse; // Creatable if not already in use for example by another axis
}

OdriveCAN::OdriveCAN() {
	inUse = true;
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

	this->port->setSpeed(250000);
	ready = true;
}

OdriveCAN::~OdriveCAN() {
	inUse = false;
}

void OdriveCAN::stopMotor(){
	this->setTorque(0.0);
}

void OdriveCAN::startMotor(){
	this->setMode(OdriveControlMode::CONTROL_MODE_TORQUE_CONTROL);
}

Encoder* OdriveCAN::getEncoder(){
	return static_cast<Encoder*>(this);
}


void OdriveCAN::sendMsg(uint8_t cmd,uint32_t value){
	CAN_tx_msg msg;

	memcpy(&msg.data,&value,4);
	msg.header.RTR = CAN_RTR_DATA;
	msg.header.DLC = 8;
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
	msg.header.DLC = 8;
	msg.header.StdId = cmd | (nodeId << 5);
	port->sendMessage(msg);
}

void OdriveCAN::requestMsg(uint8_t cmd){
	CAN_tx_msg msg;

	msg.header.RTR = CAN_RTR_REMOTE;
	msg.header.DLC = 8;
	msg.header.StdId = cmd | (nodeId << 5);
	port->sendMessage(msg);
}

float OdriveCAN::getPos_f(){
	requestMsg(0x09);
	return lastPos;
}

bool OdriveCAN::motorReady(){
	return ready;
}
int32_t OdriveCAN::getPos(){
	return 0xffff / lastPos;
}


uint32_t OdriveCAN::getCpr(){
	return 0xffff;
}
void OdriveCAN::setTorque(float torque){
	sendMsg(0x0E,torque);
}

void OdriveCAN::setMode(OdriveControlMode mode){
	sendMsg(0x0B,(uint32_t)mode);
}

void OdriveCAN::turn(int16_t power){
	float torque = ((float)power / (float)0x7fff) * maxTorque;
	this->setTorque(torque);
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
		errors = msg & 0xffffffff;
		status = (msg >> 32) & 0xffffffff;
	break;
	}
	case 0x009: // encoder pos float
	{
		uint64_t tp = msg & 0xffffffff;
		memcpy(&lastPos,&tp,sizeof(float));
		break;
	}
	case 0x0A: // encoder pos
	{
		shadowCount = msg & 0xffffffff;
		break;
	}



	default:
	break;
	}

}

#endif
