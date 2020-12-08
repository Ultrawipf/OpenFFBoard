/*
 * CanBridge.cpp
 *
 *  Created on: 20.11.2020
 *      Author: Yannick
 */


#include <CanBridge.h>
#ifdef CANBRIDGE
#include "target_constants.h"
#include "ledEffects.h"


ClassIdentifier CanBridge::info = {
		 .name = "CAN Bridge" ,
		 .id=12,
		 .hidden=false //Set false to list
 };

const ClassIdentifier CanBridge::getInfo(){
	return info;
}

CanBridge::CanBridge() {
	// Set up a filter to receive everything
	CAN_FilterTypeDef sFilterConfig;
	sFilterConfig.FilterBank = 0;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = 0x0000;
	sFilterConfig.FilterIdLow = 0x0000;
	sFilterConfig.FilterMaskIdHigh = 0x0000;
	sFilterConfig.FilterMaskIdLow = 0x0000;
	sFilterConfig.FilterFIFOAssignment = rxfifo;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.SlaveStartFilterBank = 14;

	txHeader.StdId = 12;
	txHeader.ExtId = 0;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.IDE = CAN_ID_STD;	//Use std id
	txHeader.DLC = 8;	// 8 bytes
	txHeader.TransmitGlobalTime = DISABLE;

	this->filterId = addCanFilter(CanHandle, sFilterConfig);
	// Interrupt start
}

CanBridge::~CanBridge() {
	removeCanFilter(CanHandle, filterId);
}



void CanBridge::canErrorCallback(CAN_HandleTypeDef *hcan){
	if(hcan == CanHandle){
		pulseErrLed();
	}
}

/*
 * Sends the message stored in canBuf
 */
void CanBridge::sendMessage(){
	if (HAL_CAN_AddTxMessage(CanHandle, &txHeader, txBuf, &txMailbox) != HAL_OK)
	{
	  /* Transmission request Error */
	  pulseErrLed();
	}
}

void CanBridge::sendMessage(uint32_t id, uint64_t msg){
	memcpy(txBuf,&msg,8);
	txHeader.StdId = id;
	sendMessage();
}

/*
 * Returns last received can message as string
 */
std::string CanBridge::messageToString(){
	std::string buf;
	buf = "!CAN:";
	buf += std::to_string(rxHeader.StdId);
	buf += ":";
	buf += std::to_string(*(int32_t*)this->rxBuf);
	buf += "\n";
	return buf;
}
// Can only send and receive 32bit for now
void CanBridge::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){
	if(fifo == rxfifo){
		pulseSysLed();
		memcpy(this->rxBuf,rxBuf,sizeof(this->rxBuf));
		this->rxHeader = *rxHeader;
		std::string buf = messageToString(); // Last message to string
		CDC_Transmit_FS(buf.c_str(), buf.length());


	}
}

ParseStatus CanBridge::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus flag = ParseStatus::OK; // Valid command found

	// ------------ commands ----------------
	if(cmd->cmd == "can"){ //
		if(cmd->type == CMDtype::get){
			*reply+= messageToString();
		}else if(cmd->type == CMDtype::setat){
			sendMessage(cmd->adr,cmd->val);
		}else{
			flag = ParseStatus::ERR;
		}

	}else if(cmd->cmd == "help"){
		flag = ParseStatus::OK_CONTINUE;
		*reply += "CAN commands:\ncan?<id>=<msgint> send message. Or can? (last received message)";
	}else{
		flag = ParseStatus::NOT_FOUND; // No valid command
	}
	return flag;
}

#endif
