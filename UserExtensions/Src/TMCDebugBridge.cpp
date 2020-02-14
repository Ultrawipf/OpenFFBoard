/*
 * TMCDebugBridge.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include <TMCDebugBridge.h>
#include "ledEffects.h"

// Change this
FFBoardMainIdentifier TMCDebugBridge::info = {
		 .name = "TMC Debug Bridge" ,
		 .id=11,
		 .hidden=false //Set false to list in "lsconf"
 };
// Copy this to your class for identification
const FFBoardMainIdentifier TMCDebugBridge::getInfo(){
	return info;
}

uint8_t TMCDebugBridge::checksum(std::vector<uint8_t> *buffer){
	uint16_t sum = 0;
	for(uint8_t i= 0;i<buffer->size()-1;i++){
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

bool TMCDebugBridge::executeUserCommand(ParsedCommand* cmd,std::string* reply){
	bool flag = true;
	if(cmd->cmd == "mtype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)drv->conf.motconf.motor_type);
		}else if(cmd->type == CMDtype::set){
			drv->setMotorType((MotorType)cmd->val, drv->conf.motconf.pole_pairs);
		}
	}else if(cmd->cmd == "poles"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(drv->conf.motconf.pole_pairs);
		}else if(cmd->type == CMDtype::set){
			drv->setMotorType(drv->conf.motconf.motor_type,cmd->val);
		}
	}else if(cmd->cmd == "encalign"){
		if(cmd->type == CMDtype::get){
			drv->bangInitABN(3000);
		}else if(cmd->type == CMDtype::set){
			drv->bangInitABN(cmd->val);
		}
	}else if(cmd->cmd == "ppr"){
		if(cmd->type == CMDtype::get){
			*reply += std::to_string(drv->getPpr());
		}else if(cmd->type == CMDtype::set){
			drv->setPpr(cmd->val);
		}
	}else if(cmd->cmd == "torque"){
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
	}else if(cmd->cmd == "speed"){
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
	}else{
		flag = false;
	}
	return flag;
}

void TMCDebugBridge::cdcRcv(char* Buf, uint32_t *Len){
	std::vector<uint8_t> dat;
	dat.assign(Buf,Buf+*Len);
	if(*Len == 9 && *Buf == 1){
		uint8_t crc = checksum(&dat);

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
			rx_data.push_back(checksum(&rx_data));
			CDC_Transmit_FS((char*)rx_data.data(), 9);

		}else if(cmd == 146){ // write

			uint32_t ndat;
			memcpy(&ndat,Buf+4,4);
			drv->writeReg(addr,__REV(ndat));
			std::vector<uint8_t> repl({2,1,0x64,0x92});
			repl.insert(repl.end(), dat[3], dat[7]);
			repl.push_back(checksum(&repl));
			CDC_Transmit_FS((const char*)repl.data(), 9);
		}else if(cmd == 143){
			std::vector<uint8_t> repl(8,0);
			if(addr == 3){
				repl.assign({2,1,40,0x8f,0,0,0,2});
			}else if(addr == 4){
				repl.assign({2,1,40,0x8f,2,6,2,2});
			}
			repl.push_back(checksum(&repl));
			CDC_Transmit_FS((char*)repl.data(), 9);
		}else if(cmd == 10 && addr == 5){
			std::vector<uint8_t> repl({2,1,64,0x0A,0,0,0,2});
			repl.push_back(checksum(&repl));
			CDC_Transmit_FS((const char*)repl.data(), 9);
		}else if(cmd == 0x88 && addr == 0){
			CDC_Transmit_FS("10015V306", 9);
		}else if(cmd == 0x88 && addr == 1){ // Version
			std::vector<uint8_t> repl = {2,0x01,0x64,0x88,0x00,0x0C,0x03,0x06,0x04};

			CDC_Transmit_FS((const char*)repl.data(), 9);
		}


	}else{
		if(this->parser.add(Buf, Len)){
			executeCommands(this->parser.parse());
		}
	}
}




TMCDebugBridge::TMCDebugBridge() {
	// Slow down SPI
//	tmcWriteReg(1,1); // Read HW Version
//	uint8_t buf[5];
//	tmcReadRegRaw(0, buf);
//	if(buf[2] == 1 && buf[3] == 0 && buf[4] == 0){
//		pulseClipLed();
//		this->spi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
//		HAL_SPI_Init(this->spi);
//	}
//	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_SET);
	TMC4671MainConfig tmcconf;
	tmcconf.motconf.motor_type = MotorType::STEPPER;
	tmcconf.motconf.pole_pairs = 50;
	this->drv = new TMC4671(&HSPIDRV,SPI1_SS1_GPIO_Port,SPI1_SS1_Pin,tmcconf);
	drv->initialize();
	drv->calibrateAdcOffset();
	TMC4671ABNConf encconf;
	encconf.ppr=10000;
	drv->setup_ABN_Enc(encconf);
	drv->findABNPol();
	//drv->stop();
}

TMCDebugBridge::~TMCDebugBridge() {
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_RESET);
	free(this->drv);
}

