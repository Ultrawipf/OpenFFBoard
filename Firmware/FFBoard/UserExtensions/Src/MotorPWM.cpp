/*
 * MotorPWM.cpp
 *
 *  Created on: Mar 29, 2020
 *      Author: Yannick
 */

#include <MotorPWM.h>
#ifdef PWMDRIVER

#include "cpp_target_config.h"

/*
 * Mapping of names for ModePWM_DRV
 */
const std::vector<std::string> RC_SpeedNames = {"20ms","15ms","10ms","5ms"};
const std::vector<std::string> PWM_SpeedNames = {"3khz","9khz","17khz","24khz"};
// Names of SpeedPWM_DRV
const std::vector<std::string> PwmModeNames = {"RC PPM","0%-50%-100% Centered","0-100% PWM/DIR","0-100% dual PWM"};
bool MotorPWM::pwmDriverInUse = false;

ClassIdentifier MotorPWM::info = {
		 .name = "PWM" ,
		 .id=CLSID_MOT_PWM,
 };
const ClassIdentifier MotorPWM::getInfo(){
	return info;
}

bool MotorPWM::isCreatable(){
	return !MotorPWM::pwmDriverInUse; // Creatable if not already in use for example by another axis
}


MotorPWM::MotorPWM() : CommandHandler("pwmdrv",CLSID_MOT_PWM) {

	MotorPWM::pwmDriverInUse = true;
	restoreFlash();
	//HAL_TIM_Base_Start_IT(timer);
	setPwmSpeed(pwmspeed);

	CommandHandler::registerCommands();
	registerCommand("freq", MotorPWM_commands::freq, "PWM period selection",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("mode", MotorPWM_commands::mode, "PWM mode",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("dir", MotorPWM_commands::dir, "Invert direction",CMDFLAG_GET | CMDFLAG_SET);
}

MotorPWM::~MotorPWM() {
	MotorPWM::pwmDriverInUse = false;
	HAL_TIM_PWM_Stop(timerConfig.timer, timerConfig.channel_1);
	HAL_TIM_PWM_Stop(timerConfig.timer, timerConfig.channel_2);
	HAL_TIM_PWM_Stop(timerConfig.timer, timerConfig.channel_3);
	HAL_TIM_PWM_Stop(timerConfig.timer, timerConfig.channel_4);
	HAL_TIM_Base_Stop_IT(timerConfig.timer);
}



void MotorPWM::turn(int16_t power){
	if(!active)
		return;

	if(invertDir){
		power = -power;
	}
	/*
	 * Generates a classic RC 20ms 1000-2000µs signal
	 * Centered at 1500µs for bidirectional RC ESCs and similiar stuff
	 */
	if(mode == ModePWM_DRV::RC_PWM){
		int32_t pval = power;

		float val = ((pval * 1000)/0x7fff)*tFreq;
		val = clip((1500*tFreq)-val,1000*tFreq, 2000*tFreq);
		setPWM(val,timerConfig.rcpwm_chan);

	/*
	 * Generates a 0-100% PWM signal
	 * and outputs complementary direction signals.
	 * Can be used with cheap halfbridge modules and DC motors
	 */
	}else if(mode == ModePWM_DRV::PWM_DIR){
		if(power < 0){
			setPWM(0,timerConfig.dir_chan);
			setPWM(0xffff,timerConfig.dir_chan_n);
		}else{
			setPWM(0,timerConfig.dir_chan_n);
			setPWM(0xffff,timerConfig.dir_chan);
		}
		int32_t val = (uint32_t)((abs(power) * period)/0x7fff);
		setPWM(val,timerConfig.pwm_chan);

	}else if(mode == ModePWM_DRV::CENTERED_PWM){
		int32_t pval = 0x7fff+power;
		int32_t val = (pval * period)/0xffff;
		setPWM(val,timerConfig.centerpwm_chan);

	}else if(mode == ModePWM_DRV::PWM_DUAL){
		int32_t val = (uint32_t)((abs(power) * period)/0x7fff);
		if(power < 0){
			setPWM(0,timerConfig.dualpwm1);
			setPWM(val,timerConfig.dualpwm2);
		}else{
			setPWM(0,timerConfig.dualpwm2);
			setPWM(val,timerConfig.dualpwm1);
		}
	}
}

/**
 * Calculates period and prescaler for a timer based on frequency
 */
std::pair<uint16_t,uint16_t> MotorPWM::freqToPeriodPsc(uint32_t freq){
	uint32_t arr_raw = (timerConfig.timerFreq/freq);
	uint32_t prescaler = arr_raw / 0xffff;
	uint32_t arr = arr_raw / (prescaler + 1);
	return {arr,prescaler};
}

/**
 * Setup the timer for different frequency presets.
 */
void MotorPWM::setPwmSpeed(SpeedPWM_DRV spd){
	bool ok = true;
	switch(spd){

	case SpeedPWM_DRV::LOW:
		if(mode == ModePWM_DRV::RC_PWM){
			period =  40000;  //20ms (40000/Sysclock)
			prescaler = timerConfig.timerFreq/2000000;
		}else{
			std::tie(period,prescaler) = freqToPeriodPsc(3000);
		}

	break;
	case SpeedPWM_DRV::MID:
		if(mode == ModePWM_DRV::RC_PWM){
			period = 30000;//15ms(30000/47)
			prescaler = timerConfig.timerFreq/2000000;
		}else{
			std::tie(period,prescaler) = freqToPeriodPsc(9000);
		}

	break;
	case SpeedPWM_DRV::HIGH:
		if(mode == ModePWM_DRV::RC_PWM){
			period = 20000; //10ms (20000/47)
			prescaler = timerConfig.timerFreq/2000000;
		}else{
			std::tie(period,prescaler) = freqToPeriodPsc(17000);
		}
	break;
	case SpeedPWM_DRV::VERYHIGH:
		if(mode == ModePWM_DRV::RC_PWM){
			period = 10000; //5ms (20000/23)
			prescaler = timerConfig.timerFreq/2000000;
		}else{
			std::tie(period,prescaler) = freqToPeriodPsc(24000);
		}
	break;
	default:
		ok = false;
	}

	if(ok){
		this->pwmspeed = spd;
		tFreq = (float)(timerConfig.timerFreq/1000000)/(float)(prescaler+1);

		pwmInitTimer(timerConfig.timer, timerConfig.channel_1,period,prescaler);
		pwmInitTimer(timerConfig.timer, timerConfig.channel_2,period,prescaler);
		pwmInitTimer(timerConfig.timer, timerConfig.channel_3,period,prescaler);
		pwmInitTimer(timerConfig.timer, timerConfig.channel_4,period,prescaler);
		HAL_TIM_MspPostInit(timerConfig.timer);
//		setPWM_HAL(0, timer, channel_1, period);
//		pwmInitTimer(timer, channel_2,period,prescaler);
//		setPWM_HAL(0, timer, channel_2, period);
		turn(0);
	}
}

/**
 * Updates pwm pulse length
 */
void MotorPWM::setPWM(uint32_t value,uint8_t ccr){
	if(ccr == 1){
		timerConfig.timer->Instance->CCR1 = value; // Set next CCR for channel 1
	}else if(ccr == 2){
		timerConfig.timer->Instance->CCR2 = value; // Set next CCR for channel 2
	}else if(ccr == 3){
		timerConfig.timer->Instance->CCR3 = value; // Set next CCR for channel 3
	}else if(ccr == 4){
		timerConfig.timer->Instance->CCR4 = value; // Set next CCR for channel 4
	}

}

SpeedPWM_DRV MotorPWM::getPwmSpeed(){
	return this->pwmspeed;
}




void MotorPWM::saveFlash(){
	// 0-3: mode
	// 4-6: speed
	uint16_t var = (uint8_t)this->mode & 0x7; // 1 bit free
	var |= ((uint8_t)this->pwmspeed & 0x7) << 4;
	var |= (this->invertDir & 0x1) << 15; // Last bit inversion
	Flash_Write(ADR_PWM_MODE, var);
}
void MotorPWM::restoreFlash(){
	uint16_t var = 0;
	if(Flash_Read(ADR_PWM_MODE, &var)){
		uint8_t m = var & 0xf;
		this->setMode(ModePWM_DRV(m));

		uint8_t s = (var >> 4) & 0x7;
		this->setPwmSpeed(SpeedPWM_DRV(s));

		this->invertDir = (var >> 15) & 0x1;
	}
}


void MotorPWM::startMotor(){
	active = true;
	turn(0);
}

void MotorPWM::stopMotor(){
	turn(0);
	active = false;
}


void MotorPWM::setMode(ModePWM_DRV mode){
	this->mode = mode;
	setPwmSpeed(pwmspeed); // Reinit timer
}

ModePWM_DRV MotorPWM::getMode(){
	return this->mode;
}



CommandStatus MotorPWM::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<MotorPWM_commands>(cmd.cmdId)){

	case MotorPWM_commands::freq:
	{
		if(cmd.type == CMDtype::set){
			this->setPwmSpeed((SpeedPWM_DRV)cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->getPwmSpeed());
		}else if(cmd.type == CMDtype::info){
			std::vector<std::string> names = PWM_SpeedNames;
			if(this->mode == ModePWM_DRV::RC_PWM){
				names = RC_SpeedNames;
			}else{
				names = PWM_SpeedNames;
			}
			for(uint8_t i = 0; i<names.size();i++){
				replies.emplace_back(names[i]  + ":" + std::to_string(i)+"\n");
			}
		}
		break;
	}
	case MotorPWM_commands::mode:
	{
		if(cmd.type == CMDtype::set){
			this->setMode((ModePWM_DRV)cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->getMode());
		}else if(cmd.type == CMDtype::info){
			for(uint8_t i = 0; i<PwmModeNames.size();i++){
				replies.emplace_back(PwmModeNames[i]  + ":" + std::to_string(i)+"\n");
			}
		}
		break;
	}
	case MotorPWM_commands::dir:
		return handleGetSet(cmd, replies, this->invertDir);
	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;

}



cpp_freertos::MutexStandard pwmTimMutex;
void pwmInitTimer(TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period,uint32_t prescaler){
	timer->Instance->ARR = period;
	timer->Instance->PSC = prescaler;
	TIM_OC_InitTypeDef sConfigOC = {0};
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	pwmTimMutex.Lock();
	HAL_TIM_PWM_Stop(timer, channel);
	HAL_TIM_PWM_ConfigChannel(timer, &sConfigOC, channel);
	//setPWM_HAL(0,timer,channel,period);
	HAL_TIM_PWM_Start(timer, channel);
	pwmTimMutex.Unlock();

}


/**
 * Changes the pwm value of the timer via HAL
 */
void setPWM_HAL(uint32_t value,TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period){
    TIM_OC_InitTypeDef sConfigOC;

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = value;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(timer, &sConfigOC, channel);
    HAL_TIM_PWM_Start(timer, channel);
}
#endif
