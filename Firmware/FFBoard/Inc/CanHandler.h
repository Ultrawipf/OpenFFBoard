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
#include <vector>

class CanHandler {
public:
	CanHandler();
	virtual ~CanHandler();

	static int32_t addCanFilter(CAN_HandleTypeDef* CanHandle,CAN_FilterTypeDef sFilterConfig);
	static void removeCanFilter(CAN_HandleTypeDef* CanHandle,uint8_t filterId);


	virtual void canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo);
	virtual void canErrorCallback(CAN_HandleTypeDef *hcan);
	virtual void canRxFullCallback(CAN_HandleTypeDef *hcan,uint32_t fifo);
	virtual void canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox);
	virtual void canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox);

private:
	static std::vector<CAN_FilterTypeDef> canFilters;
	static const uint8_t slaveFilterStart = 14;

};

#endif /* CANHANDLER_H_ */
#endif
