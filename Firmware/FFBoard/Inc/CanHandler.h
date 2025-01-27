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
#include "CAN.h"
// Define can types
#if defined(FDCAN1) || defined(FDCAN2)
#define CANTYPE_FDCAN
#elif defined(CAN1) || defined(CAN2)
#define CANTYPE_CAN2B
#else
#error "CAN or FDCAN must be defined"
#endif

//class CANPort;
//class CAN_msg_header_rx;
class CanHandler {
public:
	CanHandler();
	virtual ~CanHandler();
//	static std::vector<CanHandler*> canHandlers;

//	static int32_t addCanFilter(CAN_HandleTypeDef* CanHandle,CAN_FilterTypeDef sFilterConfig);
//	static void removeCanFilter(CAN_HandleTypeDef* CanHandle,uint8_t filterId);
	virtual void canRxPendCallback(CANPort* port,CAN_rx_msg& msg);
	virtual void canErrorCallback(CANPort* port,uint32_t errcode);
	virtual void canRxFullCallback(CANPort* port,uint32_t fifo);
	virtual void canTxCpltCallback(CANPort* port,uint32_t mailbox);
	virtual void canTxAbortCallback(CANPort* port,uint32_t mailbox);

//#ifdef CANTYPE_CAN2B
//	virtual void canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo);
//	virtual void canErrorCallback(CAN_HandleTypeDef *hcan);
//	virtual void canRxFullCallback(CAN_HandleTypeDef *hcan,uint32_t fifo);
//	virtual void canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox);
//	virtual void canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox);
//#elif defined(CANTYPE_FDCAN)
//	virtual void canRxPendCallback(FDCAN_HandleTypeDef *hcan,uint8_t* rxBuf,FDCAN_RxHeaderTypeDef* rxHeader,uint32_t fifo);
//	virtual void canErrorCallback(FDCAN_HandleTypeDef *hcan);
//	virtual void canRxFullCallback(FDCAN_HandleTypeDef *hcan,uint32_t fifo);
//	virtual void canTxCpltCallback(FDCAN_HandleTypeDef *hcan,uint32_t mailbox);
//	virtual void canTxAbortCallback(FDCAN_HandleTypeDef *hcan,uint32_t mailbox);
//#endif

	static std::vector<CanHandler*>& getCANHandlers() {
		static std::vector<CanHandler*> canHandlers{};
		return canHandlers;
	}
//private:
//	//static std::vector<CAN_FilterTypeDef> canFilters;
//

};

#endif /* CANHANDLER_H_ */
#endif
