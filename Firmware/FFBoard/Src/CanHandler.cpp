/*
 * CanHandler.cpp
 *
 *  Created on: 20.11.2020
 *      Author: Yannick
 */
#include "CanHandler.h"
#ifdef CANBUS



CanHandler::CanHandler() {
	extern std::vector<CanHandler*> canHandlers;
	addCallbackHandler(&canHandlers, this);

}

CanHandler::~CanHandler() {
	extern std::vector<CanHandler*> canHandlers;
	removeCallbackHandler(&canHandlers, this);
}

/*
 * RX FiFo is full
 */
void CanHandler::canRxFullCallback(CAN_HandleTypeDef *hcan,uint32_t fifo){

}

/*
 * New message received in fifo number
 */
void CanHandler::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){

}

/*
 * Error during can transfer
 */
void CanHandler::canErrorCallback(CAN_HandleTypeDef *hcan){

}

/*
 * CAN transmission complete
 */
void CanHandler::canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){

}

/*
 * CAN transmission aborted
 */
void CanHandler::canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){

}

std::vector<CAN_FilterTypeDef> CanHandler::canFilters;
/*
 * Adds a filter to the can handle
 * Returns a free bank id if successfull and -1 if all banks are full
 * Use the returned id to disable the filter again
 */
int32_t CanHandler::addCanFilter(CAN_HandleTypeDef* CanHandle,CAN_FilterTypeDef sFilterConfig){
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
		if (HAL_CAN_ConfigFilter(CanHandle, &sFilterConfig) == HAL_OK){
			canFilters.push_back(sFilterConfig);
		}
	}
	return foundId;
}

/*
 * Disables a can filter
 * Use the id returned by the addCanFilter function
 */
void CanHandler::removeCanFilter(CAN_HandleTypeDef* CanHandle,uint8_t filterId){
	for (uint8_t i = 0; i < canFilters.size(); i++){
		if(canFilters[i].FilterBank == filterId){
			HAL_CAN_ConfigFilter(CanHandle, &canFilters[i]);
			canFilters.erase(canFilters.begin()+i);
			break;
		}
	}
}

#endif
