/*
 * TMCDebugBridge.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include <TMCDebugBridge.h>
#include "ledEffects.h"
#include "voltagesense.h"
#include "cdc_device.h"

// Change this
ClassIdentifier TMCDebugBridge::info = {
		 .name = "TMC Debug Bridge" ,
		 .id=CLSID_MAIN_TMCDBG, //11
		 .visibility = ClassVisibility::debug
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

void TMCDebugBridge::registerCommands(){
	//CommandHandler::registerCommands();
	registerCommand("reg", TMCDebugBridge_commands::reg, "Read or write a TMC register at adr");
	registerCommand("torque", TMCDebugBridge_commands::torque, "Change torque and enter torque mode");
	registerCommand("pos", TMCDebugBridge_commands::pos, "Change pos and enter pos mode");
	registerCommand("openloopspeed", TMCDebugBridge_commands::openloopspeed, "Move openloop. adr=strength,val=speed");
	registerCommand("velocity", TMCDebugBridge_commands::velocity, "Change velocity and enter velocity mode");
	registerCommand("mode", TMCDebugBridge_commands::mode, "Change motion mode");
}

void TMCDebugBridge::tmcReadRegRaw(uint8_t reg,uint8_t* buf){

	uint8_t req[5] = {(uint8_t)(0x7F & reg),0,0,0,0};
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(this->spi,req,buf,5,1000);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);
}

CommandStatus TMCDebugBridge::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<TMCDebugBridge_commands>(cmd.cmdId)){
	case TMCDebugBridge_commands::torque:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(drv->getTorque()));
		}else if(cmd.type == CMDtype::set){
			drv->turn(cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case TMCDebugBridge_commands::pos:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(drv->getPos()));
		}else if(cmd.type == CMDtype::set){
			drv->setTargetPos(cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case TMCDebugBridge_commands::openloopspeed:
		if(cmd.type == CMDtype::set){
			drv->setOpenLoopSpeedAccel(cmd.val,100);
		}else if(cmd.type == CMDtype::setat){
			drv->runOpenLoop(cmd.adr,0,cmd.val,10);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case TMCDebugBridge_commands::velocity:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(drv->getVelocity()));
		}else if(cmd.type == CMDtype::set){
			drv->setTargetVelocity(cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case TMCDebugBridge_commands::mode:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply((uint8_t)drv->getMotionMode()));
		}else if(cmd.type == CMDtype::set && cmd.val < (uint8_t)MotionMode::NONE){
			drv->setMotionMode(MotionMode(cmd.val));
			drv->startMotor();
		}else{
			replies.push_back(CommandReply("stop=0,torque=1,velocity=2,position=3,prbsflux=4,prbstorque=5,prbsvelocity=6,uqudext=8,encminimove=9"));
		}
		break;

	case TMCDebugBridge_commands::reg:
		if(cmd.type == CMDtype::getat){
			replies.push_back(CommandReply(drv->readReg(cmd.val)));
		}else if(cmd.type == CMDtype::setat){
			drv->writeReg(cmd.adr,cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}

void TMCDebugBridge::sendCdc(char* dat, uint32_t len){
	tud_cdc_n_write(0, dat, len);
	tud_cdc_write_flush();
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
			sendCdc((char*)rx_data.data(), 9);

		}else if(cmd == 146){ // write

			uint32_t ndat;
			memcpy(&ndat,Buf+4,4);
			drv->writeReg(addr,__REV(ndat));
			std::vector<uint8_t> repl({2,1,0x64,0x92,dat[3],dat[4],dat[5],dat[6]});
			repl.push_back(checksum(&repl,8));
			sendCdc((char*)repl.data(), 9);
		}else if(cmd == 143){
			std::vector<uint8_t> repl(8,0);
			if(addr == 3){
				repl.assign({2,1,40,0x8f,0,0,0,2});
			}else if(addr == 4){
				repl.assign({2,1,40,0x8f,2,6,2,2});
			}
			repl.push_back(checksum(&repl,8));
			sendCdc((char*)repl.data(), 9);
		}else if(cmd == 10){ // Get global parameter.
			std::vector<uint8_t> repl({2,1,64,0x0A,0,0,0,0});
			if(addr == 5){
				repl[7] = 2; // hwid
			}else if(addr == 2){
				repl[7] = 1; // active
			}
			repl.push_back(checksum(&repl,8));
			sendCdc((char*)repl.data(), 9);
		}else if(cmd == 0x0F){ // Get input
			std::vector<uint8_t> repl({2,1,64,0x0F,0,0,0,0});
			if(addr == 5){  // Voltage
				uint16_t v = getIntV()/100;
				repl[7] = v & 0xff;
				repl[6] = (v>>8) & 0xff;
			}
			repl.push_back(checksum(&repl,8));
			sendCdc((char*)repl.data(), 9);
		}else if(cmd == 0x88){
			char version[9] = {2,'0','0','1','5','V','3','0','7'}; // Version string
			if(addr == 0){
				sendCdc(version, 9);
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
				sendCdc((char*)repl.data(), 9);
			}

		}
	}else{
		FFBoardMain::cdcRcv(Buf, Len);
	}
}




TMCDebugBridge::TMCDebugBridge() {

	TMC4671MainConfig tmcconf;
	registerCommands();

	this->drv = std::make_unique<TMC_1>();
	drv->conf = tmcconf;
	drv->setAddress(1);
	//drv->setPids(tmcpids); // load some basic pids
	drv->restoreFlash(); // before initialize!
	drv->setLimits(tmclimits);
	drv->setEncoderType(EncoderType_TMC::NONE); // Set encoder to none to prevent alignment
	//drv->initialize();
	drv->Start();
	//drv->stop();
}

TMCDebugBridge::~TMCDebugBridge() {
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_RESET);
}

