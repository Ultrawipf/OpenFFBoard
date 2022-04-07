/*
 * CanBridge.cpp
 *
 *  Created on: 20.11.2020
 *      Author: Yannick
 */


#include <CanBridge.h>
#ifdef CANBRIDGE

#include "ledEffects.h"
#include "cdc_device.h"

extern TIM_TypeDef TIM_MICROS;

ClassIdentifier CanBridge::info = {
		 .name = "CAN Bridge (GVRET)" ,
		 .id=CLSID_MAIN_CAN,
		 .visibility = ClassVisibility::visible //Set false to list
 };

const ClassIdentifier CanBridge::getInfo(){
	return info;
}

CanBridge::CanBridge() {
	this->port  = &canport;

	// Set up a filter to receive everything
	CAN_FilterTypeDef sFilterConfig;
	sFilterConfig.FilterBank = 0;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = 0x0000;
	sFilterConfig.FilterIdLow = 0x0000;
	sFilterConfig.FilterMaskIdHigh = 0x0000;
	sFilterConfig.FilterMaskIdLow = 0x0000;
	sFilterConfig.FilterFIFOAssignment = rxfifo;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.SlaveStartFilterBank = 14;

	txHeader.StdId = 12;
	txHeader.ExtId = 0;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.IDE = CAN_ID_STD;	//Use std id
	txHeader.DLC = 8;	// 8 bytes
	txHeader.TransmitGlobalTime = DISABLE;

	this->filterId = this->port->addCanFilter(sFilterConfig);
	// Interrupt start
	conf1.enabled = true;

	//CommandHandler::registerCommands();
	registerCommand("can", CanBridge_commands::can, "Send a frame or get last received frame");
	registerCommand("rtr", CanBridge_commands::canrtr, "Send a RTR frame");
	registerCommand("spd", CanBridge_commands::canspd, "Change or get CAN baud");
	this->port->setSilentMode(false);
}

CanBridge::~CanBridge() {
	this->port->removeCanFilter(filterId);
}



void CanBridge::canErrorCallback(CAN_HandleTypeDef *hcan){
	if(hcan == CanHandle){
		pulseErrLed();
	}
}

void CanBridge::sendMessage(uint32_t id, uint64_t msg,uint8_t len = 8,bool rtr = false){
	memcpy(txBuf,&msg,8);
	txHeader.StdId = id;
	txHeader.DLC = len;
	txHeader.RTR = rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA;
	if(!this->port->sendMessage(&txHeader, txBuf, &this->txMailbox)){
		pulseErrLed();
	}
}

// Can only send and receive 32bit for now
void CanBridge::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){

	if(fifo == rxfifo){

		memcpy(&lastmsg.data,rxBuf,8);
		lastmsg.header = *rxHeader;
		pulseSysLed();

		replyPending = true;
	}
}

std::string CanBridge::messageToString(CAN_rx_msg msg){
	std::string buf;
	buf = "CAN:";
	buf += std::to_string(msg.header.StdId);
	buf += ":";
	buf += std::to_string(*(int64_t*)msg.data);
	return buf;
}

void CanBridge::update(){
	if(replyPending){
		CAN_rx_msg msg = lastmsg;
		if(gvretMode){
			CAN_RxHeaderTypeDef rxHeader = msg.header;
			uint32_t time = rxHeader.Timestamp;
			uint32_t id = rxHeader.StdId;
			if(rxHeader.ExtId != 0){
				id = rxHeader.ExtId;
				id |= 0x80000000;
			}
			std::vector<char> reply = {
					0xF1,0,(char)(time & 0xff), (char)((time >> 8) & 0xff), (char)((time >> 16) & 0xff), (char)((time >> 24) & 0xff),
					(char)(id & 0xff), (char)((id >> 8) & 0xff), (char)((id >> 16) & 0xff), (char)((id >> 24) & 0xff),
					(char)((rxHeader.DLC & 0xf) | (1 >> 4))
			};

			for(uint8_t i = 0; i< rxHeader.DLC; i++){
				reply.push_back(msg.data[i]);
			}

			reply.push_back(0);
//			for(char c : reply){
//				replystr.push_back(c);
//			}
			tud_cdc_n_write(0,reply.data(), reply.size());
			tud_cdc_write_flush();
		}else{
			std::string replystr = messageToString(msg);
			CommandHandler::logSerial(replystr);

//			tud_cdc_n_write(0,replystr.c_str(), replystr.length());
//			tud_cdc_write_flush();
		}

		replyPending = false;
	}
}


void CanBridge::cdcRcv(char* Buf, uint32_t *Len){

	// Not a gvret binary command

	if(*Buf == 0xE7){
		gvretMode = true;
	}

	if(gvretMode == false || (*Buf != 0xE7 && *Buf != 0xF1)){ // Not gvret command
		FFBoardMain::cdcRcv(Buf, Len);
		return;
	}


	/*
	 * Data format in buf:
	 * 0xE1,0xE1,0xF1,cmd,data...
	 * data = buf+2
	 */

	// Find start marker
	uint8_t pos = 0;
	for(uint8_t i = 0; i < *Len-pos; i++){
		pos = i+1;
		if((Buf[i]) == 0xF1){
			break;
		}
	}

	std::vector<char> reply;
	while(pos < *Len){

		uint8_t datalength = *Len;
		uint8_t* data = (uint8_t*)(Buf+pos+1);
		uint8_t cmd = *(Buf+pos);

		// find next F1 or end
		for(uint8_t i = 0; i < *Len-pos; i++){
			datalength = i;
			if((data[i]) == 0xF1){
				break;
			}
		}
		pos = pos+datalength+2; // Move offset for next loop


		switch(cmd){

			case(0):
			{ // send can
				if(*Len < 8)
					break;
				uint32_t id = *data | (*(data+1) << 8) | (*(data+2) << 16) | (*(data+3) << 24); // = dat[2] | dat[3] >> 8 | dat[3] >> 16 | dat[4] >> 21
				uint64_t msg = 0;
				uint8_t bus = data[4];
				uint8_t msgLen = data[5];
				if(bus == 0){
					memcpy(&msg,data+6,std::min<int>(msgLen, 8)); // Copy variable length buffer
					sendMessage(id, msg,msgLen);
				}

				break;
			}
			case(1):
			{	// sync. Microseconds since start up LSB to MSB
				uint32_t time = HAL_GetTick()*1000;//HAL_CAN_GetTxTimestamp(CanHandle, txMailbox); // or use systick and scale.
				std::vector<char> t = {0xF1,cmd,(char)(time & 0xff), (char)((time >> 8) & 0xff), (char)((time >> 16) & 0xff), (char)((time >> 24) & 0xff)};
				reply.insert(reply.end(),t.begin(),t.end());

				break;
			}
			case(2): // get digital in

			break;

			case(3): // get analog in

			break;

			case(4): // set digital out

			break;

			case(5): // set can config
			{
				uint32_t speed = *data | (*(data+1) << 8) | (*(data+2) << 16) | (*(data+3) << 24);
				speed = speed & 0x1fffffff;
				uint8_t b3 = *(data+3); // config byte
				if(b3 & 0x80){
					conf1.enabled = (b3 & 0x40) != 0;
					conf1.listenOnly = (b3 & 0x20) != 0;
				}
				this->port->setSpeed(speed);
				break;
			}
			case(6): // get can config
			{
				std::vector<char> t =
					{	0xF1,cmd,
							(char)(conf1.enabled | conf1.listenOnly << 4),(char)(conf1.speed & 0xff), (char)((conf1.speed >> 8) & 0xff),(char)((conf1.speed >> 16) & 0xff),(char)((conf1.speed >> 24) & 0xff),
							(char)(conf2.enabled | conf2.listenOnly << 4),(char)(conf2.speed & 0xff), (char)((conf2.speed >> 8) & 0xff),(char)((conf2.speed >> 16) & 0xff),(char)((conf2.speed >> 24) & 0xff),
					};
				reply.insert(reply.end(),t.begin(),t.end());
				break;
			}


			case(7): // get device info
			{
				std::vector<char> t = {0xF1,cmd,1,1,1,0,0,0};
				reply.insert(reply.end(),t.begin(),t.end());
				break;
			}


			case(8): // set single wire mode

			break;

			case(9):{ // keep alive
				reply.insert(reply.end(),keepalivemsg.begin(),keepalivemsg.end());
				break;
			}
			case(10): // set system

			break;

			case(11): // echo can frame

			break;

			case(12): // Num buses
			{
				std::vector<char> t = {0xF1,cmd,numBuses};
				reply.insert(reply.end(),t.begin(),t.end());
			}
			break;
			case(13): // ext buses
			break;
			case(14): // set ext buses
			break;
			default:
				// unknown command
			break;
		}

	}
	tud_cdc_n_write(0,reply.data(), reply.size());
	tud_cdc_write_flush();
}



CommandStatus CanBridge::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<CanBridge_commands>(cmd.cmdId)){
	case CanBridge_commands::can:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(*(int64_t*)lastmsg.data,lastmsg.header.StdId));
		}else if(cmd.type == CMDtype::setat){
			sendMessage(cmd.adr,cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;


	case CanBridge_commands::canrtr:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(*(int64_t*)lastmsg.data,lastmsg.header.StdId));
		}else if(cmd.type == CMDtype::setat){
			sendMessage(cmd.adr,cmd.val,8,true); // msg with rtr bit
		}else{
			return CommandStatus::ERR;
		}
		break;

	case CanBridge_commands::canspd:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->port->getSpeed()));
		}else if(cmd.type == CMDtype::set){
			this->port->setSpeed(cmd.val);
		}
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK; // Valid command found
}

#endif
