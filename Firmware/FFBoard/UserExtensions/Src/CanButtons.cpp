/*
 * CanButtons.cpp
 *
 *  Created on: 06.04.2022
 *      Author: Yannick
 */

#include "CanButtons.h"
#include "math.h"

#ifdef CANBUTTONS
ClassIdentifier CanButtons::info = {
		 .name = "CAN Buttons" ,
		 .id=CLSID_BTN_CAN,
 };
const ClassIdentifier CanButtons::getInfo(){
	return info;
}

CanButtons::CanButtons() : CommandHandler("canbtn", CLSID_BTN_CAN, 0) {
	CommandHandler::registerCommands();
	ButtonSource::btnnum=32;
	registerCommand("btnnum", CanButtons_commands::btnnum, "Amount of buttons",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("invert", CanButtons_commands::invert, "Invert buttons",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("canid", CanButtons_commands::canid, "CAN frame ID",CMDFLAG_GET | CMDFLAG_SET);
	restoreFlash();

	setupCanPort();
}

void CanButtons::setupCanPort(){
	if(filterId != -1){
		this->port->removeCanFilter(filterId);
	}
//	CAN_FilterTypeDef sFilterConfig;
//	sFilterConfig.FilterBank = 0;
//	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
//	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
//	sFilterConfig.FilterIdHigh = (canId << 5); // Just one ID
//	sFilterConfig.FilterIdLow = 0x0000;
//	sFilterConfig.FilterMaskIdHigh = 0xFFFF;
//	sFilterConfig.FilterMaskIdLow = 0x0000;
//	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
//	sFilterConfig.FilterActivation = ENABLE;
//	sFilterConfig.SlaveStartFilterBank = 14;

	CAN_filter filterConf;
	filterConf.buffer = 0;
	filterConf.filter_id = canId;
	filterConf.filter_mask =  0xFFFF;

	this->filterId = this->port->addCanFilter(filterConf);

	//this->port->setSpeedPreset(CANSPEEDPRESET_500); // default speed used
	this->port->start();
}

CanButtons::~CanButtons() {
	if(filterId != -1)
		this->port->removeCanFilter(filterId);
}

void CanButtons::saveFlash(){
	uint16_t conf1 = (btnnum-1) & 0x3F;
	conf1 |= (invert & 0x1) << 6;
	Flash_Write(ADR_CANBTN_CONF1, conf1);

	uint16_t conf2 = canId & 0x7ff;
	Flash_Write(ADR_CANBTN_CONF2, conf2);
}

void CanButtons::restoreFlash(){
	uint16_t conf;
	if(Flash_Read(ADR_CANBTN_CONF1, &conf)){
		setBtnNum((conf & 0x3F) +1);
		invert = (conf >> 6) & 0x1;
	}
	// CAN settings
	if(Flash_Read(ADR_CANBTN_CONF2, &conf)){
		canId = conf & 0x7ff;
	}
}

void CanButtons::setBtnNum(uint8_t num){
	num = clip<uint8_t,uint8_t>(num, 1, 64); // up to 8 PCF8574 can be chained resulting in 64 buttons
	this->btnnum = num;
	if(num == 64){ // Special case
		mask = 0xffffffffffffffff;
	}else{
		mask = (uint64_t)pow<uint64_t>(2,num)-(uint64_t)1; // Must be done completely in 64 bit!
	}
}


uint8_t CanButtons::readButtons(uint64_t* buf){
	if(invert){
		*buf = ~currentButtons;
	}else{
		*buf = currentButtons;
	}
	*buf &= mask;
	return ButtonSource::btnnum;
}

CommandStatus CanButtons::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<CanButtons_commands>(cmd.cmdId)){

	case CanButtons_commands::btnnum:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->btnnum);
		}else if(cmd.type == CMDtype::set){
			setBtnNum(cmd.val);
		}
	break;
	case CanButtons_commands::invert:
		return handleGetSet(cmd, replies, this->invert);
	break;

	case CanButtons_commands::canid:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->canId);
		}else if(cmd.type == CMDtype::set){
			canId = (cmd.val) & 0x7ff;
			setupCanPort(); // Set can filter
		}
	break;

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}

void CanButtons::canRxPendCallback(CANPort* port,CAN_rx_msg& msg){

	uint32_t id = (msg.header.id) & 0x7FF;
	if(id != this->canId || msg.header.rtr || msg.header.length != 8){
		return;
	}

	currentButtons = *reinterpret_cast<uint64_t*>(msg.data);
}


#endif
