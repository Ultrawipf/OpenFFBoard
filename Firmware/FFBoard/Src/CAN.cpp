/*
 * CAN.cpp
 *
 *  Created on: 21.06.2021
 *      Author: Yannick
 */

#include "target_constants.h"
#ifdef CANBUS
#include "CAN.h"

ClassIdentifier CANPort::info = {
	.name = "Can port",
	.id = CLSID_CANPORT,
	.visibility = ClassVisibility::visible};


CANPort::CANPort(CAN_HandleTypeDef &hcan) : CommandHandler("can", CLSID_CANPORT, 0), hcan(&hcan), silentPin(nullptr) {
	//HAL_CAN_Start(this->hcan);
	restoreFlash();
#ifdef CAN_COMMANDS_DISABLED_IF_NOT_USED
	this->setCommandsEnabled(false);
#endif
	registerCommands();
}
CANPort::CANPort(CAN_HandleTypeDef &hcan,const OutputPin* silentPin) : CommandHandler("can", CLSID_CANPORT, 0), hcan(&hcan), silentPin(silentPin) {
	//HAL_CAN_Start(this->hcan);
	restoreFlash();
	registerCommands();
}

void CANPort::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("speed", CanPort_commands::speed, "CAN speed preset (0:50k;1:100k;2:125k;3:250k;4:500k;5:1M)", CMDFLAG_GET|CMDFLAG_SET|CMDFLAG_INFOSTRING);
	registerCommand("send", CanPort_commands::send, "Send CAN frame. Adr&Value required", CMDFLAG_SETADR);
	registerCommand("len", CanPort_commands::len, "Set length of next frames", CMDFLAG_SET|CMDFLAG_GET);
}

void CANPort::saveFlash(){
	if(this->getCommandHandlerInfo()->instance != 0){
		return; // Only first instance can save
	}
	uint16_t data = (this->speedPreset & 0b111); // set the baudrate in 0..2 bit
	Flash_Write(ADR_CANCONF1, data);
}

void CANPort::restoreFlash(){
	if(this->getCommandHandlerInfo()->instance != 0){
		return; // Only first instance can save
	}
	uint16_t data;
	if(Flash_Read(ADR_CANCONF1, &data)){
		setSpeedPreset(data & 0b111);
	}
}


CANPort::~CANPort() {
	// removes all filters
	for (uint8_t i = 0; i < canFilters.size(); i++){
		canFilters[i].FilterActivation = false;
		HAL_CAN_ConfigFilter(this->hcan, &canFilters[i]);
	}
	canFilters.clear();
	HAL_CAN_Stop(this->hcan);
	if(silentPin){
		silentPin->set(); // set pin high to disable
	}
}

/**
 * Signals that this port is being used.
 * Increments the user counter
 */
void CANPort::takePort(){
	if(portUsers++ == 0){
		start();
	}
}

/**
 * Signals that the port is not needed anymore.
 * Decrements the user counter
 */
void CANPort::freePort(){
	if(portUsers>0){
		portUsers--;
	}

	if(portUsers == 0){
		stop();
	}
}

/**
 * Enables the can port
 */
bool CANPort::start(){
	setSilentMode(false);
	active = true;
#ifdef CAN_COMMANDS_DISABLED_IF_NOT_USED
	this->setCommandsEnabled(true);
#endif
	//CAN_IT_RX_FIFO0_FULL | CAN_IT_RX_FIFO0_OVERRUN
	HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_RX_FIFO0_FULL | CAN_IT_RX_FIFO0_OVERRUN | CAN_IT_RX_FIFO1_FULL | CAN_IT_RX_FIFO1_OVERRUN | CAN_IT_ERROR | CAN_IT_BUSOFF | CAN_IT_TX_MAILBOX_EMPTY);
	setSpeedPreset(this->speedPreset); // Set preset again for a safe state
	return HAL_CAN_Start(this->hcan) == HAL_OK;
}

/**
 * Disables the can port
 */
bool CANPort::stop(){
	setSilentMode(true);
	active = false;
#ifdef CAN_COMMANDS_DISABLED_IF_NOT_USED
	this->setCommandsEnabled(false);
#endif
	return HAL_CAN_Start(this->hcan) == HAL_OK;
}


uint32_t CANPort::getSpeed(){
	return presetToSpeed(speedPreset);
}

uint8_t CANPort::getSpeedPreset(){
	return (speedPreset);
}

/**
 * Converts a numeric speed in bits/s to the matching preset ID
 */
uint8_t CANPort::speedToPreset(uint32_t speed){
	uint8_t preset = 255;
	switch(speed){

		case 50000:
			preset = CANSPEEDPRESET_50;
		break;

		case 100000:
			preset = CANSPEEDPRESET_100;
		break;

		case 125000:
			preset = CANSPEEDPRESET_125;
		break;

		case 250000:
			preset = CANSPEEDPRESET_250;
		break;

		case 500000:
			preset = CANSPEEDPRESET_500;
		break;

		case 1000000:
			preset = CANSPEEDPRESET_1000;
		break;
		default:

		break;
	}
	return preset;
}

/**
 * Converts a preset to bits/s
 */
uint32_t CANPort::presetToSpeed(uint8_t preset){
	uint32_t speed = 0;
	switch(preset){
		case CANSPEEDPRESET_50:
			speed = 50000;
		break;
		case CANSPEEDPRESET_100:
			speed = 100000;
		break;
		case CANSPEEDPRESET_125:
			speed = 125000;
		break;
		case CANSPEEDPRESET_250:
			speed = 250000;
		break;
		case CANSPEEDPRESET_500:
			speed = 500000;
		break;
		case CANSPEEDPRESET_1000:
			speed = 1000000;
		break;
		default:
		break;
	}
	return speed;
}

/**
 * Changes the speed of the CAN port to a preset
 */
void CANPort::setSpeedPreset(uint8_t preset){
	if(preset > 5)
		return;
	speedPreset = preset;

	configSem.Take();
	HAL_CAN_Stop(this->hcan);
	HAL_CAN_AbortTxRequest(hcan, txMailboxes);
	this->hcan->Instance->BTR = canSpeedBTR_preset[preset];
	HAL_CAN_ResetError(hcan);

	HAL_CAN_Start(this->hcan);
	configSem.Give();
}

/**
 * Changes the speed of the CAN port in bits/s
 * Must match a preset speed
 */
void CANPort::setSpeed(uint32_t speed){
	uint8_t preset = speedToPreset(speed);
	setSpeedPreset(preset);
}

bool CANPort::takeSemaphore(uint32_t delay){
	bool isIsr = inIsr();
	BaseType_t taskWoken = 0;
	bool success;
	if(isIsr)
		success = this->semaphore.TakeFromISR(&taskWoken);
	else{
		success = this->semaphore.Take(delay);
	}
	//isTakenFlag = true;
	portYIELD_FROM_ISR(taskWoken);
	return success;
}

void CANPort::giveSemaphore(){
	bool isIsr = inIsr();
	BaseType_t taskWoken = 0;
	if(isIsr)
		this->semaphore.GiveFromISR(&taskWoken);
	else
		this->semaphore.Give();
	isWaitingFlag = false;
	portYIELD_FROM_ISR(taskWoken);
}

/**
 * Sets the can port passive if a transceiver with silent mode is available
 */
void CANPort::setSilentMode(bool silent){
	this->silent = silent;
	if(silentPin){
		silentPin->write(silent);
	}
}

/**
 * Transmits a CAN frame on this port
 * Wraps the internal transmit function
 */
bool CANPort::sendMessage(CAN_tx_msg& msg){
	return this->sendMessage(&msg.header,msg.data,nullptr);
}

void CANPort::abortTxRequests(){
	HAL_CAN_AbortTxRequest(hcan, txMailboxes);
}

/**
 * Transmits a CAN frame with separate data and header settings
 */
bool CANPort::sendMessage(CAN_TxHeaderTypeDef *pHeader, uint8_t aData[],uint32_t *pTxMailbox){
	if(this->silent)
		setSilentMode(false);
	uint32_t mailbox;
	if(pTxMailbox == nullptr){
		pTxMailbox = &mailbox;
	}

	if(!HAL_CAN_GetTxMailboxesFreeLevel(hcan) && HAL_GetTick() - lastSentTime > sendTimeout){
		// Mailbox full and nothing has been sent successfully for some time. Abort previous requests if timeout reached
		HAL_CAN_AbortTxRequest(hcan, txMailboxes);
	}
//
	if(!HAL_CAN_GetTxMailboxesFreeLevel(hcan)){ // Only wait if no mailbox is free
		isWaitingFlag = true;
		if(!takeSemaphore(sendTimeout)){
			isWaitingFlag = false;
			return false;
		}

	}

	//this->isTakenFlag = true;
	if (HAL_CAN_AddTxMessage(this->hcan, pHeader, aData, pTxMailbox) != HAL_OK)
	{
	  /* Transmission request Error */
		if(isWaitingFlag)
			giveSemaphore();
	  return false;
	}
	txMailboxes |= *pTxMailbox;
//	if(HAL_CAN_GetTxMailboxesFreeLevel(hcan)) // Give back semaphore immediately if mailboxes are still free
//		giveSemaphore();
	return true;
}

void CANPort::canTxCpltCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){
	if(hcan == this->hcan){
		lastSentTime = HAL_GetTick();
		txMailboxes &= ~mailbox;
		if(isWaitingFlag)
			giveSemaphore();
	}
}

void CANPort::canTxAbortCallback(CAN_HandleTypeDef *hcan,uint32_t mailbox){
	if(hcan == this->hcan){
		txMailboxes &= ~mailbox;
		if(isWaitingFlag)
			giveSemaphore();
	}
}

void CANPort::canErrorCallback(CAN_HandleTypeDef *hcan){
	if(
		hcan == this->hcan &&
		hcan->ErrorCode & (HAL_CAN_ERROR_TX_ALST0 | HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_TX_ALST2 | HAL_CAN_ERROR_TX_TERR0 | HAL_CAN_ERROR_TX_TERR1 | HAL_CAN_ERROR_TX_TERR2)
	){
		giveSemaphore();
	}
}


/**
 * Adds a filter to the can handle
 * Returns a free bank id if successfull and -1 if all banks are full
 * Use the returned id to disable the filter again
 */
int32_t CANPort::addCanFilter(CAN_FilterTypeDef sFilterConfig){
	configSem.Take();
	int32_t lowestId = 0;// sFilterConfig.FilterFIFOAssignment == CAN_RX_FIFO0 ? 0 : slaveFilterStart;
	int32_t highestId = slaveFilterStart;// sFilterConfig.FilterFIFOAssignment == CAN_RX_FIFO0 ? slaveFilterStart : 29;
	int32_t foundId = -1;

	for(uint8_t id = lowestId; id < highestId ; id++ ){
		bool foundExisting = false;
		for(CAN_FilterTypeDef filter : canFilters){
			if(id == filter.FilterBank
//					&& filter.FilterIdHigh == sFilterConfig.FilterIdHigh && filter.FilterIdLow == sFilterConfig.FilterIdLow &&filter.FilterFIFOAssignment == sFilterConfig.FilterFIFOAssignment &&
//					&& filter.FilterMaskIdHigh == sFilterConfig.FilterMaskIdHigh && filter.FilterMaskIdLow == sFilterConfig.FilterMaskIdLow
//					&& filter.FilterMode == sFilterConfig.FilterMode && filter.FilterScale == sFilterConfig.FilterScale
					)
			{
				foundExisting = true;
				break;
			}
		}
		foundId = id;
		if(!foundExisting){
			break;
		}

	}
	if(foundId < highestId){
		if(sFilterConfig.FilterBank == 0)
			sFilterConfig.FilterBank = foundId;
		if (HAL_CAN_ConfigFilter(this->hcan, &sFilterConfig) == HAL_OK){
			canFilters.push_back(sFilterConfig);
		}
	}
	configSem.Give();
	return foundId;
}

/**
 * Disables a can filter
 * Use the id returned by the addCanFilter function
 */
void CANPort::removeCanFilter(uint8_t filterId){
	configSem.Take();
	for (uint8_t i = 0; i < canFilters.size(); i++){
		if(canFilters[i].FilterBank == filterId){
			canFilters[i].FilterActivation = false;
			HAL_CAN_ConfigFilter(this->hcan, &canFilters[i]);
			canFilters.erase(canFilters.begin()+i);
			break;
		}
	}
	configSem.Give();
}


CommandStatus CANPort::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<CanPort_commands>(cmd.cmdId)){

	case CanPort_commands::speed:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->speedPreset);
		}else if(cmd.type == CMDtype::set){
			setSpeedPreset(cmd.val);
		}else if(cmd.type == CMDtype::info){
			for(uint8_t i = 0; i<SpeedNames.size();i++){
				replies.emplace_back(SpeedNames[i]  + ":" + std::to_string(i));
			}
		}
	break;

	case CanPort_commands::send:
	{
		if(cmd.type == CMDtype::setat){
			if(!active){
				start(); // If port is not used activate port at first use
			}
			CAN_tx_msg msg;
			memcpy(msg.data,&cmd.val,8);
			header.StdId = cmd.adr;
			msg.header = header;
			sendMessage(msg);
		}else{
			return CommandStatus::NOT_FOUND;
		}
		break;
	}
	case CanPort_commands::len:
		handleGetSet(cmd, replies, header.DLC);
		header.DLC = std::min<uint32_t>(header.DLC,8);
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}

#endif
