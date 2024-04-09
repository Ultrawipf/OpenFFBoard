/*
 * EncoderLocal.cpp
 *
 *  Created on: 02.02.2020
 *      Author: Yannick
 */

#include "EncoderLocal.h"
#include "flash_helpers.h"
#ifdef LOCALENCODER
bool EncoderLocal::inUse = false;
ClassIdentifier EncoderLocal::info = {
		 .name = "Local ABN" ,
		 .id=CLSID_ENCODER_LOCAL,
 };
const ClassIdentifier EncoderLocal::getInfo(){
	return info;
}

EncoderLocal::EncoderLocal() : CommandHandler("localenc",CLSID_ENCODER_LOCAL) {
	EncoderLocal::inUse = true;
	this->restoreFlash();
	this->htim = &TIM_ENC;
	HAL_TIM_Base_Start_IT(htim); // May immediately call overflow. Initialize count again
	this->htim->Instance->CNT = 0x7fff;
	pos = 0;
	this->htim->Instance->CR1 = 1;

	if(!useIndex)
		setPos(0);

	registerCommands();
}

void EncoderLocal::registerCommands(){
	CommandHandler::registerCommands();
	registerCommand("cpr", EncoderLocal_commands::cpr, "CPR of encoder",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("index", EncoderLocal_commands::useindex, "Enable index pin",CMDFLAG_GET|CMDFLAG_SET);
}

EncoderLocal::~EncoderLocal() {
	EncoderLocal::inUse = false;
	this->htim->Instance->CR1 = 0;
}


void EncoderLocal::saveFlash(){
	Flash_Write(ADR_ENCLOCAL_CPR, this->getCpr());
	Flash_Write(ADR_ENCLOCAL_OFS, useIndex ? (int16_t)this->offset : 0); // 0 offset indicates index off

}

void EncoderLocal::restoreFlash(){
	uint16_t cpr = 0;
	if (Flash_Read(ADR_ENCLOCAL_CPR, &cpr)){
		this->setCpr(cpr);
	}
	uint16_t ofs = 0;
	if(Flash_Read(ADR_ENCLOCAL_OFS, &ofs)){
		this->offset = (int16_t)ofs;
		useIndex = ofs != 0;
	}

}

int32_t EncoderLocal::getTimerCount(){
	return (int32_t)htim->Instance->CNT - (int32_t)0x7fff;
}

EncoderType EncoderLocal::getEncoderType(){
	return EncoderType::incremental;
}


int32_t EncoderLocal::getPos(){
	return getTimerCount() + pos;
}

void EncoderLocal::setPos(int32_t pos){
	int32_t cnt = getTimerCount();

	this->pos = pos - cnt;
	if(useIndex && indexHit){
		int32_t idxpos = ((int32_t)indexpos - (int32_t)0x7fff);
		offset = ( ( idxpos-cnt-pos ) % this->cpr); // offset is distance to index position (0 = 0x7fff index)
	}
	//this->pos = pos - cnt;
	//htim->Instance->CNT = 0x7fff; // Reset //pos+0x7fff;


}


void EncoderLocal::setPeriod(uint32_t period){
	this->htim->Instance->ARR = period-1;
}

void EncoderLocal::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == ENCODER_Z_Pin){
		if(HAL_GPIO_ReadPin(ENCODER_Z_GPIO_Port, ENCODER_Z_Pin) == GPIO_PIN_RESET){
		// Encoder Z pin activated

			if(useIndex && !indexHit){
				pulseSysLed();
				htim->Instance->CNT = 0x7fff; // Center encoder
				this->pos = offset;
			}
			indexpos = htim->Instance->CNT; // Store current encoder count
			indexHit = true;
		}

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
	case EncoderLocal_commands::useindex:
		return handleGetSet(cmd, replies, useIndex);
	break;
	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
}
#endif
