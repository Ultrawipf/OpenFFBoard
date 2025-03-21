/*
 * CANPort2B.h
 *
 *  Created on: 21.09.2023
 *      Author: Yannick
 */

#ifndef CANPORT2B_H_
#define CANPORT2B_H_
#include "CAN.h"
#include "CanHandler.h"

#if defined(CANTYPE_2B)
class CANPort;
class CANPort_2B : public CANPort, public CommandHandler,public CanHandler{
	enum class CanPort_commands : uint32_t {speed,send,len};
public:
	CANPort_2B(CAN_HandleTypeDef &hcan,const CANPortHardwareConfig& presets,const OutputPin* silentPin=nullptr,uint8_t instance = 0);
	virtual ~CANPort_2B();

	bool start();
	bool stop();


	bool sendMessage(CAN_tx_msg& msg);
	bool sendMessage(CAN_msg_header_tx *pHeader, uint8_t aData[],uint32_t *pTxMailbox = nullptr);
	void abortTxRequests();

	int32_t addCanFilter(CAN_filter filter);
	void removeCanFilter(uint8_t filterId);

	void setSpeed(uint32_t speed);
	void setSpeedPreset(uint8_t preset);
	uint32_t getSpeed();
	uint8_t getSpeedPreset();

	void setSilentMode(bool silent);

	static const uint8_t slaveFilterStart = 14;

	void canTxCpltCallback(CANPort *port,uint32_t mailbox) override;
	void canTxAbortCallback(CANPort *port,uint32_t mailbox) override;
	void canErrorCallback(CANPort *port, uint32_t code) override;

	// Config

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands();
	void saveFlash();
	void restoreFlash();
	static ClassIdentifier info;
	const ClassIdentifier getInfo(){return this->info;}
	const ClassType getClassType() override {return ClassType::Port;};
	void* getHandle(){return this->hcan;}

private:
	uint8_t speedPreset = speedToPreset(500000); // default
	CAN_HandleTypeDef *hcan = nullptr;
	std::vector<CAN_FilterTypeDef> canFilters;
	uint32_t txMailboxes = 0;


	const OutputPin* silentPin;
	bool silent = true;
	bool active = false;

	uint32_t lastSentTime = 0;

	static const uint32_t sendTimeout = 20;

	CAN_TxHeaderTypeDef header = {0,0,0,CAN_RTR_DATA,8,(FunctionalState)0};
	uint8_t nextLen = 8;

};
#endif

#endif /* CANPORTFDCAN_H_ */
