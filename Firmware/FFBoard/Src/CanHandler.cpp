/*
 * CanHandler.cpp
 *
 *  Created on: 20.11.2020
 *      Author: Yannick
 */
#include "CanHandler.h"
#ifdef CANBUS

//std::vector<CanHandler*> CanHandler::canHandlers;

CanHandler::CanHandler() {
	addCallbackHandler(getCANHandlers(), this);

}

CanHandler::~CanHandler() {
	removeCallbackHandler(getCANHandlers(), this);
}

/**
 * RX FiFo is full
 */
void CanHandler::canRxFullCallback(CAN_HandleTypeDef *hcan,uint32_t fifo){

}

/**
 * New message received in fifo number
 */
void CanHandler::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){

}

/**
 * Error during can transfer
 */
void CanHandler::canErrorCallback(CAN_HandleTypeDef *hcan){

}

/**
 * CAN transmission complete
 */
void CanHandler::canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){

}

/**
 * CAN transmission aborted
 */
void CanHandler::canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){

}



#endif
