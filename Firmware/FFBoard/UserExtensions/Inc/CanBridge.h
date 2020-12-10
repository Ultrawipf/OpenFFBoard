/*
 * CanBridge.h
 *
 *  Created on: 20.11.2020
 *      Author: Yannick
 */


#ifndef CANBRIDGE_H_
#define CANBRIDGE_H_

#include "target_constants.h"
#ifdef CANBRIDGE
#include <FFBoardMain.h>
#include "cppmain.h"
#include "CanHandler.h"

extern "C"{
#include "stm32f4xx_hal_can.h"
}

/*
 * A main class for testing CAN communication.
 * Sends CAN messages via CDC and can transmit CAN messages with 4 bytes
 */
class CanBridge: public FFBoardMain, public CanHandler {
public:

	CanBridge();
	virtual ~CanBridge();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	ParseStatus command(ParsedCommand* cmd,std::string* reply);
	void sendMessage(uint32_t id, uint64_t msg);
	void sendMessage();
	void canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo);
	void canErrorCallback(CAN_HandleTypeDef *hcan);
	std::string messageToString();
	CAN_HandleTypeDef* CanHandle = &CANPORT;


private:
	int32_t filterId = -1;

	CAN_TxHeaderTypeDef txHeader;
	CAN_RxHeaderTypeDef rxHeader; // Receive header

	uint8_t rxBuf[8] = {0};
	uint8_t txBuf[8] = {0};
	uint32_t txMailbox;
	uint32_t rxfifo = CAN_RX_FIFO0;
};
#endif /* CANBRIDGE_H_ */
#endif
