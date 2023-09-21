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
void CanHandler::canRxFullCallback(CANPort* port,uint32_t fifo){

}

/**
 * New message received in fifo number
 */
void CanHandler::canRxPendCallback(CANPort* port,CAN_rx_msg& msg){

}

/**
 * Error during can transfer
 */
void CanHandler::canErrorCallback(CANPort* port,uint32_t errcode){

}

/**
 * CAN transmission complete
 */
void CanHandler::canTxCpltCallback(CANPort* port,uint32_t mailbox){

}

/**
 * CAN transmission aborted
 */
void CanHandler::canTxAbortCallback(CANPort* port,uint32_t mailbox){

}



#endif
