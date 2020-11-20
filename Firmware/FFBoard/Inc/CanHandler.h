/*
 * CanHandler.h
 *
 *  Created on: 20.11.2020
 *      Author: Yannick
 */
#include "target_constants.h"
#ifdef CANBUS
#ifndef CANHANDLER_H_
#define CANHANDLER_H_

#include "cppmain.h"
#include "global_callbacks.h"

class CanHandler {
public:
	CanHandler();
	virtual ~CanHandler();

	virtual void canRxPendCallback(CAN_HandleTypeDef *hcan,uint32_t fifo);
	virtual void canErrorCallback(CAN_HandleTypeDef *hcan);
	virtual void canRxFullCallback(CAN_HandleTypeDef *hcan,uint32_t fifo);
	virtual void canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox);
	virtual void canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox);
};

#endif /* CANHANDLER_H_ */
#endif
