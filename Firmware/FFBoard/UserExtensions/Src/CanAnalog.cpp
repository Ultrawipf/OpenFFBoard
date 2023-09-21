/*
 * CanAnalog.cpp
 *
 *  Created on: 06.04.2022
 *      Author: Yannick
 */

#include <CanAnalog.h>
#include "math.h"

#ifdef CANANALOG
ClassIdentifier CanAnalogBase::info = {
		 .name = "CAN Analog" ,
		 .id=CLSID_ANALOG_CAN,
 };
const ClassIdentifier CanAnalogBase::getInfo(){
	return info;
}

CanAnalogBase::CanAnalogBase(uint8_t maxAxes) : CommandHandler("cananalog", CLSID_ANALOG_CAN, 0), maxAxes(maxAxes) {
	CommandHandler::registerCommands();
	registerCommand("canid", CanAnalog_commands::canid, "CAN frame ID of first packet. Next packet ID+1",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("amount", CanAnalog_commands::amount, "Amount of analog axes",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("maxamount", CanAnalog_commands::maxAmount, "Maxmimum amount of analog axes",CMDFLAG_GET);
	restoreFlash();

	setupCanPort();
	setAxisNum(axes);
}

void CanAnalogBase::setupCanPort(){
	if(filterId != -1){
		this->port->removeCanFilter(filterId);
	}
//	CAN_FilterTypeDef sFilterConfig;
//	sFilterConfig.FilterBank = 0;
//	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
//	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
//	sFilterConfig.FilterIdHigh = (canId << 5);
//	sFilterConfig.FilterIdLow = 0x0000;
//	sFilterConfig.FilterMaskIdHigh = ~((canId << 5) ^ ((canId+1) << 5)); // Mask is inverted xor of the IDs
//	sFilterConfig.FilterMaskIdLow = 0x0000;
//	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
//	sFilterConfig.FilterActivation = ENABLE;
//	sFilterConfig.SlaveStartFilterBank = 14;
	CAN_filter filterConf;
	filterConf.buffer = 0;
	filterConf.filter_id = canId;
	filterConf.filter_mask =  ~((canId) ^ ((canId+1) ));
	this->filterId = this->port->addCanFilter(filterConf);
	//this->port->setSpeedPreset(CANSPEEDPRESET_500); // default speed used
	this->port->start();
}

CanAnalogBase::~CanAnalogBase() {
	if(filterId != -1)
		this->port->removeCanFilter(filterId);
}

void CanAnalogBase::saveFlash(){
	uint16_t conf1 = canId & 0x7FF;
	conf1 |= (axes & 0xF) << 11;
	Flash_Write(ADR_CANANALOG_CONF1, conf1);


}

void CanAnalogBase::restoreFlash(){
	uint16_t conf;

	// CAN settings
	if(Flash_Read(ADR_CANANALOG_CONF1, &conf)){
		canId = conf & 0x7ff;
		setAxisNum((conf >> 11) & 0xF);
	}
}

void CanAnalogBase::setAxisNum(uint8_t num){
	axes = std::min(num,maxAxes);
	packets = ((axes-1)/4) +1;
	this->buf.resize(axes,0);
}




CommandStatus CanAnalogBase::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<CanAnalog_commands>(cmd.cmdId)){

	case CanAnalog_commands::amount:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->axes);
		}else if(cmd.type == CMDtype::set){
			setAxisNum(cmd.val);
		}
	break;

	case CanAnalog_commands::maxAmount:
		replies.emplace_back(this->maxAxes);
		break;

	case CanAnalog_commands::canid:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->canId);
		}else if(cmd.type == CMDtype::set){
			canId = std::min<uint32_t>((cmd.val) & 0x7ff,0x7ff-(this->axes/4));
			setupCanPort(); // Set can filter
		}
	break;

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}

void CanAnalogBase::canRxPendCallback(CANPort* port,CAN_rx_msg& msg){

	uint32_t id = (msg.header.id) & 0x7FF;
//	pulseClipLed();
	if(msg.header.rtr || msg.header.length != 8){
		return;
	}

	for(uint8_t packet = 0; packet < packets ; packet++){
		if(id != this->canId+packet){
			continue;
		}
		for(uint8_t i = 0; i< axes-(packet*4) ; i++){
			this->buf[i+(packet*4)] = msg.data[i*2] | (msg.data[i*2+1] << 8);
		}
	}
}


#endif
