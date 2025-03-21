/*
 * CANPort2B.cpp
 *
 *  Created on: 21.09.2023
 *      Author: Yannick
 */

#include "CANPort2B.h"
#if defined(CANTYPE_2B)
ClassIdentifier CANPort_2B::info = {
	.name = "Can port",
	.id = CLSID_CANPORT,
	.visibility = ClassVisibility::visible};

//CANPort::CANPort(const CANPortHardwareConfig<uint32_t>& presets,uint8_t instance) : presets(presets),CommandHandler("can", CLSID_CANPORT, instance){
//
//}

CANPort_2B::CANPort_2B(CAN_HandleTypeDef &hcan,const CANPortHardwareConfig& presets,const OutputPin* silentPin,uint8_t instance) : CANPort(presets),CommandHandler("can", CLSID_CANPORT, instance), hcan(&hcan),silentPin(silentPin) {
	//HAL_CAN_Start(this->hcan);
	restoreFlashDelayed();
#ifdef CAN_COMMANDS_DISABLED_IF_NOT_USED
	this->setCommandsEnabled(false);
#endif
	registerCommands();
}
//CANPort_2B::CANPort(CAN_HandleTypeDef &hcan,const OutputPin* silentPin) : CommandHandler("can", CLSID_CANPORT, 0), hcan(&hcan), silentPin(silentPin) {
//	//HAL_CAN_Start(this->hcan);
//	restoreFlash();
//	registerCommands();
//}

void CANPort_2B::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("speed", CanPort_commands::speed, "CAN speed preset (! for list)", CMDFLAG_GET|CMDFLAG_SET|CMDFLAG_INFOSTRING);
	registerCommand("send", CanPort_commands::send, "Send CAN frame. Adr&Value required", CMDFLAG_SETADR);
	registerCommand("len", CanPort_commands::len, "Set length of next frames", CMDFLAG_SET|CMDFLAG_GET);
}

void CANPort_2B::saveFlash(){
	if(this->getCommandHandlerInfo()->instance != 0){
		return; // Only first instance can save
	}
	uint16_t data = (this->speedPreset & 0b111); // set the baudrate in 0..2 bit
	Flash_Write(ADR_CANCONF1, data);
}

void CANPort_2B::restoreFlash(){
	if(this->getCommandHandlerInfo()->instance != 0){
		return; // Only first instance can save
	}
	uint16_t data;
	if(Flash_Read(ADR_CANCONF1, &data)){
		setSpeedPreset(data & 0b111);
	}
}


CANPort_2B::~CANPort_2B() {
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
//void CANPort_2B::takePort(){
//	if(portUsers++ == 0){
//		start();
//	}
//}

/**
 * Signals that the port is not needed anymore.
 * Decrements the user counter
 */
//void CANPort_2B::freePort(){
//	if(portUsers>0){
//		portUsers--;
//	}
//
//	if(portUsers == 0){
//		stop();
//	}
//}

/**
 * Enables the can port
 */
bool CANPort_2B::start(){
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
bool CANPort_2B::stop(){
	setSilentMode(true);
	active = false;
#ifdef CAN_COMMANDS_DISABLED_IF_NOT_USED
	this->setCommandsEnabled(false);
#endif
	return HAL_CAN_Stop(this->hcan) == HAL_OK;
}


uint32_t CANPort_2B::getSpeed(){
	return presetToSpeed(speedPreset);
}

uint8_t CANPort_2B::getSpeedPreset(){
	return (speedPreset);
}


/**
 * Changes the speed of the CAN port to a preset
 */
void CANPort_2B::setSpeedPreset(uint8_t preset){
	if(preset > 5)
		return;
	speedPreset = preset;

	configSem.Take();
	HAL_CAN_Stop(this->hcan);
	HAL_CAN_AbortTxRequest(hcan, txMailboxes);
	this->hcan->Instance->BTR = presets.getPreset(preset).init;
	HAL_CAN_ResetError(hcan);

	HAL_CAN_Start(this->hcan);
	configSem.Give();
}

/**
 * Changes the speed of the CAN port in bits/s
 * Must match a preset speed
 */
void CANPort_2B::setSpeed(uint32_t speed){
	uint8_t preset = speedToPreset(speed);
	setSpeedPreset(preset);
}



/**
 * Sets the can port passive if a transceiver with silent mode is available
 */
void CANPort_2B::setSilentMode(bool silent){
	this->silent = silent;
	if(silentPin){
		silentPin->write(silent);
	}
}

/**
 * Transmits a CAN frame on this port
 * Wraps the internal transmit function
 */
bool CANPort_2B::sendMessage(CAN_tx_msg& msg){
	return this->sendMessage(&msg.header,msg.data,nullptr);
}

void CANPort_2B::abortTxRequests(){
	HAL_CAN_AbortTxRequest(hcan, txMailboxes);
}

/**
 * Transmits a CAN frame with separate data and header settings
 */
bool CANPort_2B::sendMessage(CAN_msg_header_tx *pHeader, uint8_t aData[],uint32_t *pTxMailbox){

	header.DLC = pHeader->length;
	header.IDE = pHeader->extId ? CAN_ID_EXT : CAN_ID_STD;
	header.RTR = pHeader->rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA;
	header.StdId = pHeader->extId ? 0 : pHeader->id;
	header.ExtId = pHeader->extId ? pHeader->id : 0;

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
	if (HAL_CAN_AddTxMessage(this->hcan, &header, aData, pTxMailbox) != HAL_OK)
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

void CANPort_2B::canTxCpltCallback(CANPort *port,uint32_t mailbox){
	if(port == this){
		lastSentTime = HAL_GetTick();
		txMailboxes &= ~mailbox;
		if(isWaitingFlag)
			giveSemaphore();
	}
}

void CANPort_2B::canTxAbortCallback(CANPort *port,uint32_t mailbox){
	if(port == this){
		txMailboxes &= ~mailbox;
		if(isWaitingFlag)
			giveSemaphore();
	}
}

void CANPort_2B::canErrorCallback(CANPort *port, uint32_t code){
	if(
		port == this &&
		code & (HAL_CAN_ERROR_TX_ALST0 | HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_TX_ALST2 | HAL_CAN_ERROR_TX_TERR0 | HAL_CAN_ERROR_TX_TERR1 | HAL_CAN_ERROR_TX_TERR2)
	){
		giveSemaphore();
	}
}


/**
 * Adds a filter to the can handle
 * Returns a free bank id if successfull and -1 if all banks are full
 * Use the returned id to disable the filter again
 */
int32_t CANPort_2B::addCanFilter(CAN_filter filter){

	CAN_FilterTypeDef sFilterConfig;
	sFilterConfig.FilterActivation = filter.active;
	sFilterConfig.FilterBank = 0; // Init new
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterFIFOAssignment = filter.buffer == 0 ? CAN_RX_FIFO0 : CAN_RX_FIFO1;
	sFilterConfig.FilterIdHigh = ((filter.filter_id << 5)  | (filter.filter_id >> (32 - 5))) & 0xFFFF;
	sFilterConfig.FilterIdLow = (filter.filter_id >> (11 - 3)) & 0xFFF8;
	sFilterConfig.FilterMaskIdHigh = ((filter.filter_mask << 5)  | (filter.filter_mask >> (32 - 5))) & 0xFFFF;
	sFilterConfig.FilterMaskIdLow = (filter.filter_mask >> (11 - 3)) & 0xFFF8;


	if(filter.extid){
		sFilterConfig.FilterIdLow |= 0x04;
		sFilterConfig.FilterMaskIdLow |= 0x4; // Add IDE bit
	}

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
void CANPort_2B::removeCanFilter(uint8_t filterId){
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


CommandStatus CANPort_2B::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<CanPort_commands>(cmd.cmdId)){

	case CanPort_commands::speed:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->speedPreset);
		}else if(cmd.type == CMDtype::set){
			setSpeedPreset(cmd.val);
		}else if(cmd.type == CMDtype::info){
			for(uint8_t i = 0; i<presets.presets.size();i++){
				replies.emplace_back(std::string(presets.presets[i].name)  + ":" + std::to_string(i));
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
			msg.header.id = cmd.adr;
			msg.header.length = nextLen;
			sendMessage(msg);
		}else{
			return CommandStatus::NOT_FOUND;
		}
		break;
	}
	case CanPort_commands::len:
		handleGetSet(cmd, replies, nextLen);
		nextLen = std::min<uint32_t>(nextLen,8);
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}
#endif
