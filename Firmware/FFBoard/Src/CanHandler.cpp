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
void CanHandler::canRxPendCallback(CAN_HandleTypeDef *hcan,uint32_t fifo){

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

#endif
