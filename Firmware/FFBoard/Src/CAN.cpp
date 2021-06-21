/*
 * CAN.cpp
 *
 *  Created on: 21.06.2021
 *      Author: Yannick
 */

#include "target_constants.h"
#ifdef CANBUS
#include "CAN.h"


CANPort::CANPort(CAN_HandleTypeDef &hcan) : hcan(&hcan) {
	//HAL_CAN_Start(this->hcan);
}

CANPort::~CANPort() {
	// removes all filters
	for (uint8_t i = 0; i < canFilters.size(); i++){
		canFilters[i].FilterActivation = false;
		HAL_CAN_ConfigFilter(this->hcan, &canFilters[i]);
	}
	canFilters.clear();
	HAL_CAN_Stop(this->hcan);
}


uint32_t CANPort::getSpeed(){
	return presetToSpeed(speedPreset);
}

uint8_t CANPort::getSpeedPreset(){
	return (speedPreset);
}

uint8_t CANPort::speedToPreset(uint32_t speed){
	uint8_t preset = 255;
	switch(speed){

		case 50000:
			preset = CANSPEEDPRESET_50;
		break;

		case 100000:
			preset = CANSPEEDPRESET_100;
		break;

		case 125000:
			preset = CANSPEEDPRESET_125;
		break;

		case 250000:
			preset = CANSPEEDPRESET_250;
		break;

		case 500000:
			preset = CANSPEEDPRESET_500;
		break;

		case 1000000:
			preset = CANSPEEDPRESET_1000;
		break;
		default:

		break;
	}
	return preset;
}

uint32_t CANPort::presetToSpeed(uint8_t preset){
	uint32_t speed = 0;
	switch(preset){
		case CANSPEEDPRESET_50:
			speed = 50000;
		break;
		case CANSPEEDPRESET_100:
			speed = 100000;
		break;
		case CANSPEEDPRESET_125:
			speed = 125000;
		break;
		case CANSPEEDPRESET_250:
			speed = 250000;
		break;
		case CANSPEEDPRESET_500:
			speed = 500000;
		break;
		case CANSPEEDPRESET_1000:
			speed = 1000000;
		break;
		default:
		break;
	}
	return speed;
}

void CANPort::setSpeedPreset(uint8_t preset){
	if(preset > 5 || preset == this->speedPreset)
		return;
	speedPreset = preset;

	takeSemaphore();
	HAL_CAN_Stop(this->hcan);

	this->hcan->Instance->BTR = canSpeedBTR_preset[preset];

	HAL_CAN_Start(this->hcan);
	giveSemaphore();
}


void CANPort::setSpeed(uint32_t speed){
	uint8_t preset = speedToPreset(speed);
	setSpeedPreset(preset);
}

void CANPort::takeSemaphore(){
	bool isIsr = inIsr();
	BaseType_t taskWoken = 0;
	if(isIsr)
		this->semaphore.TakeFromISR(&taskWoken);
	else
		this->semaphore.Take();
	isTakenFlag = true;
	portYIELD_FROM_ISR(taskWoken);
}

void CANPort::giveSemaphore(){
	bool isIsr = inIsr();
	BaseType_t taskWoken = 0;
	if(isIsr)
		this->semaphore.GiveFromISR(&taskWoken);
	else
		this->semaphore.Give();
	isTakenFlag = false;
	portYIELD_FROM_ISR(taskWoken);
}

bool CANPort::sendMessage(CAN_tx_msg msg){
	return this->sendMessage(&msg.header,msg.data,&this->txMailbox);
}

bool CANPort::sendMessage(CAN_TxHeaderTypeDef *pHeader, uint8_t aData[],uint32_t *pTxMailbox){
	if(pTxMailbox == nullptr){
		pTxMailbox = &this->txMailbox;
	}
	takeSemaphore();
	this->isTakenFlag = true;
	if (HAL_CAN_AddTxMessage(this->hcan, pHeader, aData, pTxMailbox) != HAL_OK)
	{
	  /* Transmission request Error */
	  giveSemaphore();
	  return false;
	}
	giveSemaphore();
	return true;
}

///**
// * RX FiFo is full
// */
//void CANPort::canRxFullCallback(CAN_HandleTypeDef *hcan,uint32_t fifo){
//	if(hcan != this->hcan){
//		return;
//	}
//}
//
///**
// * New message received in fifo number
// */
//void CANPort::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){
//	if(hcan != this->hcan){
//		return;
//	}
//}
//
///**
// * Error during can transfer
// */
//void CANPort::canErrorCallback(CAN_HandleTypeDef *hcan){
//	if(hcan != this->hcan){
//		return;
//	}
//}
//
///**
// * CAN transmission complete
// */
//void CANPort::canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){
//	if(hcan != this->hcan){
//		return;
//	}
//
//}
//
///**
// * CAN transmission aborted
// */
//void CANPort::canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){
//	if(hcan != this->hcan){
//		return;
//	}
//}


/*
 * Adds a filter to the can handle
 * Returns a free bank id if successfull and -1 if all banks are full
 * Use the returned id to disable the filter again
 */
int32_t CANPort::addCanFilter(CAN_FilterTypeDef sFilterConfig){
	takeSemaphore();
	int32_t lowestId = sFilterConfig.FilterFIFOAssignment == CAN_RX_FIFO0 ? 0 : slaveFilterStart;
	int32_t highestId = sFilterConfig.FilterFIFOAssignment == CAN_RX_FIFO0 ? slaveFilterStart : 29;
	int32_t foundId = -1;
	for(uint8_t id = lowestId; id < highestId ; id++ ){
		for(CAN_FilterTypeDef filter : canFilters){
			if(filter.FilterFIFOAssignment == sFilterConfig.FilterFIFOAssignment && id == filter.FilterBank){
				break;
			}
		}
		foundId = id;
		break;
	}
	if(foundId < highestId){
		if (HAL_CAN_ConfigFilter(this->hcan, &sFilterConfig) == HAL_OK){
			canFilters.push_back(sFilterConfig);
		}
	}
	giveSemaphore();
	return foundId;
}

/*
 * Disables a can filter
 * Use the id returned by the addCanFilter function
 */
void CANPort::removeCanFilter(uint8_t filterId){
	semaphore.Take();
	for (uint8_t i = 0; i < canFilters.size(); i++){
		if(canFilters[i].FilterBank == filterId){
			canFilters[i].FilterActivation = false;
			HAL_CAN_ConfigFilter(this->hcan, &canFilters[i]);
			canFilters.erase(canFilters.begin()+i);
			break;
		}
	}
	semaphore.Give();
}
#endif
