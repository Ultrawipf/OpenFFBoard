/*
 * TMCDebugBridge.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include <TMCDebugBridge.h>
#include "ledEffects.h"
#include "voltagesense.h"
// Change this
ClassIdentifier TMCDebugBridge::info = {
		 .name = "TMC Debug Bridge" ,
		 .id=11,
		 .hidden=false
 };
// Copy this to your class for identification
const ClassIdentifier TMCDebugBridge::getInfo(){
	return info;
}

uint8_t TMCDebugBridge::checksum(std::vector<uint8_t> *buffer,uint8_t len){
	uint16_t sum = 0;
	for(uint8_t i = 0; i < len ;i++){
		sum+= (*buffer)[i];
	}
	sum = sum % 256;
	return sum;
}

void TMCDebugBridge::tmcReadRegRaw(uint8_t reg,uint8_t* buf){

	uint8_t req[5] = {(uint8_t)(0x7F & reg),0,0,0,0};
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(this->spi,req,buf,5,1000);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);
}

ParseStatus TMCDebugBridge::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus flag = ParseStatus::OK;
	if(cmd->cmd == "torque"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(drv->getTorque());
		}else if(cmd->type == CMDtype::set){
			drv->turn(cmd->val);
		}
	}else if(cmd->cmd == "pos"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(drv->getPos());
		}else if(cmd->type == CMDtype::set){
			drv->setTargetPos(cmd->val);
		}
	}else if(cmd->cmd == "openloopspeed"){
		if(cmd->type == CMDtype::set){
			drv->setOpenLoopSpeedAccel(cmd->val,100);

		}else if(cmd->type == CMDtype::setat){
			drv->runOpenLoop(cmd->adr,0,cmd->val,10);
		}
	}else if(cmd->cmd == "velocity"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(drv->getVelocity());
		}else if(cmd->type == CMDtype::set){
			drv->setTargetVelocity(cmd->val);
		}
	}else if(cmd->cmd == "mode"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)drv->getMotionMode());
		}else if(cmd->type == CMDtype::set && cmd->val < (uint8_t)MotionMode::NONE){
			drv->setMotionMode(MotionMode(cmd->val));
		}else{
			*reply+="stop=0,torque=1,velocity=2,position=3,prbsflux=4,prbstorque=5,prbsvelocity=6,uqudext=8,encminimove=9";
		}
	}else if(cmd->cmd == "reg"){
		if(cmd->type == CMDtype::getat){
			*reply+=std::to_string(drv->readReg(cmd->val));
		}else if(cmd->type == CMDtype::setat){
			drv->writeReg(cmd->adr,cmd->val);
		}
	}else if(cmd->cmd == "help"){
		flag = ParseStatus::OK_CONTINUE;
		*reply += "TMC Debug:torque,  openloopspeed,pos, velocity,mode,reg\n";
	}else{
		flag = ParseStatus::NOT_FOUND;
	}
	return flag;
}

void TMCDebugBridge::cdcRcv(char* Buf, uint32_t *Len){
	std::vector<uint8_t> dat;
	dat.assign(Buf,Buf+*Len);
	if(*Len == 9 && *Buf == 1){
		uint8_t crc = checksum(&dat,*Len-1);

		//uint8_t id = dat[0];
	    uint8_t cmd = dat[1];
		uint8_t addr = dat[2];

		if(dat[dat.size()-1] != crc){
			//fail
			return;
		}

		if(cmd == 148){ // read reg
			uint8_t buf[5];
			uint8_t rpl[4] = {2,1,0x64,0x94};
			tmcReadRegRaw(addr, buf);
			std::vector<uint8_t> rx_data;

			rx_data.insert(rx_data.begin(), &rpl[0],&rpl[4]);
			rx_data.insert(rx_data.end(),buf+1, buf+5);
			rx_data.push_back(checksum(&rx_data,8));
			CDC_Transmit_FS((char*)rx_data.data(), 9);

		}else if(cmd == 146){ // write

			uint32_t ndat;
			memcpy(&ndat,Buf+4,4);
			drv->writeReg(addr,__REV(ndat));
			std::vector<uint8_t> repl({2,1,0x64,0x92,dat[3],dat[4],dat[5],dat[6]});
			repl.push_back(checksum(&repl,8));
			CDC_Transmit_FS((char*)repl.data(), 9);
		}else if(cmd == 143){
			std::vector<uint8_t> repl(8,0);
			if(addr == 3){
				repl.assign({2,1,40,0x8f,0,0,0,2});
			}else if(addr == 4){
				repl.assign({2,1,40,0x8f,2,6,2,2});
			}
			repl.push_back(checksum(&repl,8));
			CDC_Transmit_FS((char*)repl.data(), 9);
		}else if(cmd == 10){ // Get global parameter.
			std::vector<uint8_t> repl({2,1,64,0x0A,0,0,0,0});
			if(addr == 5){
				repl[7] = 2; // hwid
			}else if(addr == 2){
				repl[7] = 1; // active
			}
			repl.push_back(checksum(&repl,8));
			CDC_Transmit_FS((char*)repl.data(), 9);
		}else if(cmd == 0x0F){ // Get input
			std::vector<uint8_t> repl({2,1,64,0x0F,0,0,0,0});
			if(addr == 5){  // Voltage
				uint16_t v = getIntV()/100;
				repl[7] = v & 0xff;
				repl[6] = (v>>8) & 0xff;
			}
			repl.push_back(checksum(&repl,8));
			CDC_Transmit_FS((char*)repl.data(), 9);
		}else if(cmd == 0x88){
			char version[9] = {2,'0','0','1','5','V','3','0','7'}; // Version string
			if(addr == 0){
				CDC_Transmit_FS(version, 9);
			}else if(addr == 1){ // Version binary
				// module version high
				uint32_t tmpVal = (uint8_t) version[1] - '0';
				std::vector<uint8_t> repl = {2,0x01,0x64,0x88,0x00,0,0,0};
				tmpVal *= 10;
				tmpVal += (uint8_t) version[2] - '0';
				repl[4] = tmpVal;

				// module version low
				tmpVal = (uint8_t) version[3] - '0';
				tmpVal *= 10;
				tmpVal += (uint8_t) version[4] - '0';
				repl[5] = tmpVal;

				// fw version high
				repl[6] = (uint8_t) version[6] - '0';

				// fw version low
				tmpVal = (uint8_t) version[7] - '0';
				tmpVal *= 10;
				tmpVal += (uint8_t) version[8] - '0';
				repl[7] = tmpVal;
				repl.push_back(checksum(&repl,8));
				CDC_Transmit_FS((char*)repl.data(), 9);
			}

		}
	}else{
		FFBoardMain::cdcRcv(Buf, Len);
	}
}




TMCDebugBridge::TMCDebugBridge() {

	TMC4671MainConfig tmcconf;

	this->drv = new TMC4671(&HSPIDRV,SPI1_SS1_GPIO_Port,SPI1_SS1_Pin,tmcconf);
	drv->setAddress(1);
	drv->restoreFlash();
	drv->initialize();
	//drv->stop();
}

TMCDebugBridge::~TMCDebugBridge() {
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_RESET);
	delete this->drv;
}

