/*
 * EncoderLocal.cpp
 *
 *  Created on: 02.02.2020
 *      Author: Yannick
 */

#include "EncoderLocal.h"
#include "flash_helpers.h"

bool EncoderLocal::inUse = false;
ClassIdentifier EncoderLocal::info = {
		 .name = "Local" ,
		 .id=CLSID_ENCODER_LOCAL,
 };
const ClassIdentifier EncoderLocal::getInfo(){
	return info;
}

EncoderLocal::EncoderLocal() : CommandHandler("localenc",CLSID_ENCODER_LOCAL) {
	EncoderLocal::inUse = true;
	this->restoreFlash();
	this->htim = &TIM_ENC;
	this->htim->Instance->CR1 = 1;
	HAL_TIM_Base_Start_IT(htim);
	setPos(0);
	registerCommands();
}

void EncoderLocal::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("cpr", EncoderLocal_commands::cpr, "CPR of encoder",CMDFLAG_GET|CMDFLAG_SET);
}

EncoderLocal::~EncoderLocal() {
	EncoderLocal::inUse = false;
	this->htim->Instance->CR1 = 0;
}


void EncoderLocal::saveFlash(){
	Flash_Write(ADR_ENCLOCAL_CPR, this->getCpr());
}

void EncoderLocal::restoreFlash(){
	uint16_t cpr = 0;
	if (Flash_Read(ADR_ENCLOCAL_CPR, &cpr)){
		this->setCpr(cpr);
	}
}

EncoderType EncoderLocal::getEncoderType(){
	return EncoderType::incremental;
}


int32_t EncoderLocal::getPos(){
	int32_t timpos = htim->Instance->CNT - (int32_t)0x7fff;
	return timpos + pos;
}
void EncoderLocal::setPos(int32_t pos){
	this->pos = pos;
	htim->Instance->CNT = pos+0x7fff;
}



void EncoderLocal::setPeriod(uint32_t period){
	this->htim->Instance->ARR = period-1;
}

void EncoderLocal::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == ENCODER_Z_Pin){
		// Encoder Z pin activated
	}
}

void EncoderLocal::timerElapsed(TIM_HandleTypeDef* htim){
	if(htim == this->htim){
		overflowCallback();
	}
}

void EncoderLocal::overflowCallback(){
	if(htim->Instance->CNT > this->htim->Instance->ARR/2){
		pos -= htim->Instance->ARR+1;
	}else{
		pos += htim->Instance->ARR+1;
	}
}

void EncoderLocal::setCpr(uint32_t cpr){
	this->cpr = cpr;
}

CommandStatus EncoderLocal::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<EncoderLocal_commands>(cmd.cmdId)){
	case EncoderLocal_commands::cpr:
		handleGetFuncSetFunc(cmd, replies, &EncoderLocal::getCpr,&EncoderLocal::setCpr,this);
	break;
	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
//	if(cmd->cmd == "cpr"){
//		if(cmd->type == CMDtype::get){
//			*reply += std::to_string(this->getCpr());
//		}else if(cmd->type == CMDtype::set){
//			this->setCpr(cmd->val);
//		}
//	}else{
//		return ParseStatus::NOT_FOUND;
//	}
//	return ParseStatus::OK;
}
