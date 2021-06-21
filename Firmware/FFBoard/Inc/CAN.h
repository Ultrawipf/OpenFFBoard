/*
 * CAN.h
 *
 *  Created on: 21.06.2021
 *      Author: Yannick
 */

#ifndef SRC_CAN_H_
#define SRC_CAN_H_
#include "target_constants.h"
#ifdef CANBUS
#include "CanHandler.h"
#include "main.h"
#include <vector>
#include "semaphore.hpp"

typedef struct{
	uint8_t data[8] = {0};
	CAN_TxHeaderTypeDef header = {0,0,0,0,8,(FunctionalState)0};
} CAN_tx_msg;

typedef struct{
	uint8_t data[8] = {0};
	CAN_RxHeaderTypeDef header = {0,0,0,0,0,0};
} CAN_rx_msg;



class CANPort { //  : public CanHandler if interrupt callbacks needed
public:
	CANPort(CAN_HandleTypeDef &hcan);
	virtual ~CANPort();

	bool sendMessage(CAN_tx_msg msg);
	bool sendMessage(CAN_TxHeaderTypeDef *pHeader, uint8_t aData[],uint32_t *pTxMailbox = nullptr);

	int32_t addCanFilter(CAN_FilterTypeDef sFilterConfig);
	void removeCanFilter(uint8_t filterId);

	void setSpeed(uint32_t speed);
	void setSpeedPreset(uint8_t preset);
	uint32_t getSpeed();
	uint8_t getSpeedPreset();

	void giveSemaphore();
	void takeSemaphore();

//	void canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo);
//	void canErrorCallback(CAN_HandleTypeDef *hcan);
//	void canRxFullCallback(CAN_HandleTypeDef *hcan,uint32_t fifo);
//	void canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox);
//	void canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox);

	static const uint8_t slaveFilterStart = 14;

	static uint8_t speedToPreset(uint32_t speed);
	static uint32_t presetToSpeed(uint8_t preset);

private:
	uint8_t speedPreset = CANSPEEDPRESET_500; // default
	bool isTakenFlag = false;
	CAN_HandleTypeDef *hcan = nullptr;
	std::vector<CAN_FilterTypeDef> canFilters;
	uint32_t txMailbox;
	cpp_freertos::BinarySemaphore semaphore = cpp_freertos::BinarySemaphore(true);
};

#endif /* SRC_CAN_H_ */
#endif
