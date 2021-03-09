/*
 * CanBridge.cpp
 *
 *  Created on: 20.11.2020
 *      Author: Yannick
 */


#include <CanBridge.h>
#ifdef CANBRIDGE
#include "target_constants.h"
#include "ledEffects.h"
#include "cdc_device.h"

extern TIM_TypeDef TIM_MICROS;

ClassIdentifier CanBridge::info = {
		 .name = "CAN Bridge" ,
		 .id=12,
		 .hidden=false //Set false to list
 };

const ClassIdentifier CanBridge::getInfo(){
	return info;
}

CanBridge::CanBridge() {
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

	this->filterId = addCanFilter(CanHandle, sFilterConfig);
	// Interrupt start
	conf1.enabled = true;
}

CanBridge::~CanBridge() {
	removeCanFilter(CanHandle, filterId);
}



void CanBridge::canErrorCallback(CAN_HandleTypeDef *hcan){
	if(hcan == CanHandle){
		pulseErrLed();
	}
}

/*
 * Sends the message stored in canBuf
 */
void CanBridge::sendMessage(){
	if (HAL_CAN_AddTxMessage(CanHandle, &txHeader, txBuf, &txMailbox) != HAL_OK)
	{
	  /* Transmission request Error */
	  pulseErrLed();
	}
}

void CanBridge::sendMessage(uint32_t id, uint64_t msg,uint8_t len = 8){
	memcpy(txBuf,&msg,8);
	txHeader.StdId = id;
	txHeader.DLC = len;
	sendMessage();
}

/*
 * Returns last received can message as string
 */
std::string CanBridge::messageToString(){
	std::string buf;
	buf = "!CAN:";
	buf += std::to_string(rxHeader.StdId);
	buf += ":";
	buf += std::to_string(*(int32_t*)this->rxBuf);
	buf += "\n";
	return buf;
}

void CanBridge::setCanSpeed(uint32_t speed){
	HAL_CAN_Stop(this->CanHandle);
	this->speed = speed;
	switch(speed){

	case 50000:
		this->CanHandle->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_50];
	break;

	case 100000:
		this->CanHandle->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_100];
	break;

	case 125000:
		this->CanHandle->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_125];
	break;

	case 250000:
		this->CanHandle->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_250];
	break;

	case 500000:
		this->CanHandle->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_500];
	break;

	case 1000000:
		this->CanHandle->Instance->BTR = canSpeedBTR_preset[CANSPEEDPRESET_1000];
	break;


	}

	HAL_CAN_Start(this->CanHandle);
}

// Can only send and receive 32bit for now
void CanBridge::canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo){
	if(fifo == rxfifo){
		pulseSysLed();

		if(gvretMode){
			uint32_t time = rxHeader->Timestamp;
			uint32_t id = rxHeader->StdId;
			if(rxHeader->ExtId != 0){
				id = rxHeader->ExtId;
				id |= 0x80000000;
			}
			std::vector<char> reply = {
					0xF1,0,(char)(time & 0xff), (char)((time >> 8) & 0xff), (char)((time >> 16) & 0xff), (char)((time >> 24) & 0xff),
					(char)(id & 0xff), (char)((id >> 8) & 0xff), (char)((id >> 16) & 0xff), (char)((id >> 24) & 0xff),
					(char)((rxHeader->DLC & 0xf) | (1 >> 4))
			};
			for(uint8_t i = 0; i< rxHeader->DLC; i++){
				reply.push_back(*(rxBuf+i));
			}
			reply.push_back(0);
			tud_cdc_n_write(0,reply.data(), reply.size());
			tud_cdc_write_flush();
		}else{
			memcpy(this->rxBuf,rxBuf,sizeof(this->rxBuf));
			this->rxHeader = *rxHeader;
			std::string buf = messageToString(); // Last message to string
			tud_cdc_n_write(0,buf.c_str(), buf.length());
			tud_cdc_write_flush();
		}

	}
}


//void CanBridge::sendGvretReply(std::vector<uint8_t>* dat, uint8_t cmd){
//	CDC_Transmit_FS(data, len);
//}

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
				this->setCanSpeed(speed);
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


//void CanBridge::sendGvretReply(std::vector<uint8_t>* dat, uint8_t cmd){
//	CDC_Transmit_FS(data, len);
//}

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
				this->setCanSpeed(speed);
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

		if(gvretMode){
			uint32_t time = rxHeader->Timestamp;
			uint32_t id = rxHeader->StdId;
			if(rxHeader->ExtId != 0){
				id = rxHeader->ExtId;
				id |= 0x80000000;
			}
			std::vector<char> reply = {
					0xF1,0,(char)(time & 0xff), (char)((time >> 8) & 0xff), (char)((time >> 16) & 0xff), (char)((time >> 24) & 0xff),
					(char)(id & 0xff), (char)((id >> 8) & 0xff), (char)((id >> 16) & 0xff), (char)((id >> 24) & 0xff),
					(char)((rxHeader->DLC & 0xf) | (1 >> 4))
			};
			for(uint8_t i = 0; i< rxHeader->DLC; i++){
				reply.push_back(*(rxBuf+i));
			}
			reply.push_back(0);
			CDC_Transmit_FS(reply.data(), reply.size());

		}else{
			memcpy(this->rxBuf,rxBuf,sizeof(this->rxBuf));
			this->rxHeader = *rxHeader;
			std::string buf = messageToString(); // Last message to string
			CDC_Transmit_FS(buf.c_str(), buf.length());

		}

	}
}


//void CanBridge::sendGvretReply(std::vector<uint8_t>* dat, uint8_t cmd){
//	CDC_Transmit_FS(data, len);
//}

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
	tud_cdc_n_write(0,reply.data(), reply.size());
	tud_cdc_write_flush();
}

ParseStatus CanBridge::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus flag = ParseStatus::OK; // Valid command found

	// ------------ commands ----------------
	if(cmd->cmd == "can"){ //
		if(cmd->type == CMDtype::get){
			*reply+= messageToString();
		}else if(cmd->type == CMDtype::setat){
			sendMessage(cmd->adr,cmd->val);
		}else{
			flag = ParseStatus::ERR;
		}

	}else if(cmd->cmd == "canspd"){
		if(cmd->type == CMDtype::get){
			*reply += std::to_string(this->speed);
		}else if(cmd->type == CMDtype::set){
			this->setCanSpeed(cmd->val);
		}
	}else{
		flag = ParseStatus::NOT_FOUND; // No valid command
	}
	return flag;
}

#endif
