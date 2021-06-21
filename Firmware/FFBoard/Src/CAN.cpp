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
	return this->speed;
}

void CANPort::setSpeed(uint32_t speed){
	semaphore.Take();
	HAL_CAN_Stop(this->hcan);
	this->speed = speed;
	switch(speed){

	case 50000:
		this->hcan->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_50];
	break;

	case 100000:
		this->hcan->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_100];
	break;

	case 125000:
		this->hcan->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_125];
	break;

	case 250000:
		this->hcan->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_250];
	break;

	case 500000:
		this->hcan->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_500];
	break;

	case 1000000:
		this->hcan->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_1000];
	break;


	}

	HAL_CAN_Start(this->hcan);
	semaphore.Give();
}

bool CANPort::sendMessage(CAN_tx_msg msg){
	return this->sendMessage(&msg.header,msg.data,&this->txMailbox);
}

bool CANPort::sendMessage(CAN_TxHeaderTypeDef *pHeader, uint8_t aData[],uint32_t *pTxMailbox){
	if(pTxMailbox == nullptr){
		pTxMailbox = &this->txMailbox;
	}
	semaphore.Take();
	this->isTakenFlag = true;
	if (HAL_CAN_AddTxMessage(this->hcan, pHeader, aData, pTxMailbox) != HAL_OK)
	{
	  /* Transmission request Error */
	  semaphore.Give();
	  return false;
	}
	semaphore.Give();
	return true;
}

/*
 * RX FiFo is full
 */
void CANPort::canRxFullCallback(CAN_HandleTypeDef *hcan,uint32_t fifo){
	if(hcan != this->hcan){
		return;
	}
}

/*
 * New message received in fifo number
 */
void CANPort::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){
	if(hcan != this->hcan){
		return;
	}
}

/*
 * Error during can transfer
 */
void CANPort::canErrorCallback(CAN_HandleTypeDef *hcan){
	if(hcan != this->hcan){
		return;
	}
}

/*
 * CAN transmission complete
 */
void CANPort::canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){
	if(hcan != this->hcan){
		return;
	}

}

/*
 * CAN transmission aborted
 */
void CANPort::canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){
	if(hcan != this->hcan){
		return;
	}
}


/*
 * Adds a filter to the can handle
 * Returns a free bank id if successfull and -1 if all banks are full
 * Use the returned id to disable the filter again
 */
int32_t CANPort::addCanFilter(CAN_FilterTypeDef sFilterConfig){
	semaphore.Take();
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
	semaphore.Give();
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
