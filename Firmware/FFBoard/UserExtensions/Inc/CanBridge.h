/*
 * CanBridge.h
 *
 *  Created on: 20.11.2020
 *      Author: Yannick
 */


#ifndef CANBRIDGE_H_
#define CANBRIDGE_H_

#include "cpp_target_config.h"
#ifdef CANBRIDGE
#include <FFBoardMain.h>
#include "cppmain.h"
#include "CanHandler.h"
#include "CAN.h"

extern "C"{
#include "stm32f4xx_hal_can.h"
}


typedef struct{
	bool listenOnly = false;
	uint32_t speed = 500000;
	bool enabled = false;
} CAN_Config_t;



/**
 * A main class for testing CAN communication.
 * Sends CAN messages via CDC and can transmit CAN messages with 4 bytes
 */
class CanBridge: public FFBoardMain, public CanHandler {
	enum class CanBridge_commands : uint32_t{
		can,canrtr,canspd
	};
public:

	CanBridge();
	virtual ~CanBridge();

	static ClassIdentifier info;
	const ClassIdentifier getInfo() override;
	static bool isCreatable() {return true;};
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void sendMessage(uint32_t id, uint64_t msg,uint8_t len,bool rtr);
	void sendMessage();
	void canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo);
	void canErrorCallback(CAN_HandleTypeDef *hcan);
	void cdcRcv(char* Buf, uint32_t *Len) override;
	CAN_HandleTypeDef* CanHandle = &CANPORT;

	std::string messageToString(CAN_rx_msg msg);

	void update(); // Main loop
	volatile bool replyPending = false;

	void registerCommands();
	virtual std::string getHelpstring(){return "CAN commands:\ncan=(msgint)?(id) send message (canrtr with rtr bit). Or can? (last received message). canspd (speed).\nThis class is GVRET/SavvyCAN compatible!";}


private:
	CANPort* port = &canport;
	std::vector<CAN_rx_msg> rxmessages;
	CAN_rx_msg lastmsg;
	int32_t filterId = -1;
	const uint8_t numBuses = 1;


	CAN_TxHeaderTypeDef txHeader;

	uint8_t txBuf[8] = {0};
	uint32_t txMailbox;
	uint32_t rxfifo = CAN_RX_FIFO0;
	CAN_Config_t conf1;
	CAN_Config_t conf2;
	bool gvretMode = false;
	const std::vector<char> keepalivemsg = {0xF1,0x09, 0xDE,0xAD};
};
#endif /* CANBRIDGE_H_ */
#endif
