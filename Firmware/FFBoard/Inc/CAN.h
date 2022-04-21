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
#include "OutputPin.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"

typedef struct{
	uint8_t data[8] = {0};
	CAN_TxHeaderTypeDef header = {0,0,0,CAN_RTR_DATA,8,(FunctionalState)0};
} CAN_tx_msg;

typedef struct{
	uint8_t data[8] = {0};
	CAN_RxHeaderTypeDef header = {0,0,0,0,0,0};
} CAN_rx_msg;



class CANPort : public CommandHandler, public PersistentStorage { //  : public CanHandler if interrupt callbacks needed
	enum class CanPort_commands : uint32_t {speed,send};
public:
	CANPort(CAN_HandleTypeDef &hcan);
	CANPort(CAN_HandleTypeDef &hcan,const OutputPin* silentPin);
	virtual ~CANPort();

	bool start();
	bool stop();

	void takePort();
	void freePort();
	int32_t getPortUsers(){return portUsers;}

	bool sendMessage(CAN_tx_msg& msg);
	bool sendMessage(CAN_TxHeaderTypeDef *pHeader, uint8_t aData[],uint32_t *pTxMailbox = nullptr);

	int32_t addCanFilter(CAN_FilterTypeDef sFilterConfig);
	void removeCanFilter(uint8_t filterId);

	void setSpeed(uint32_t speed);
	void setSpeedPreset(uint8_t preset);
	uint32_t getSpeed();
	uint8_t getSpeedPreset();

	void giveSemaphore();
	void takeSemaphore();

	void setSilentMode(bool silent);

	static const uint8_t slaveFilterStart = 14;

	static uint8_t speedToPreset(uint32_t speed);
	static uint32_t presetToSpeed(uint8_t preset);


	// Config

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands();
	void saveFlash();
	void restoreFlash();
	static ClassIdentifier info;
	const ClassIdentifier getInfo(){return this->info;}
	const ClassType getClassType() override {return ClassType::Port;};

private:
	uint8_t speedPreset = CANSPEEDPRESET_500; // default
	CAN_HandleTypeDef *hcan = nullptr;
	std::vector<CAN_FilterTypeDef> canFilters;
	uint32_t txMailbox;
	cpp_freertos::BinarySemaphore semaphore = cpp_freertos::BinarySemaphore(true);
	//bool isTakenFlag = false;
	const OutputPin* silentPin;
	bool silent = true;
	bool active = false;
	int32_t portUsers = 0;

	const std::vector<std::string> SpeedNames = {"50k","100k","125k","250k","500k","1000k"};

};

#endif /* SRC_CAN_H_ */
#endif
