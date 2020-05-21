/*
 * MotorPWM.cpp
 *
 *  Created on: Mar 29, 2020
 *      Author: Yannick
 */


/*
 * Contains motor drivers based on PWM generation
 */
#include <MotorPWM.h>

/*
 * Mapping of names for ModePWM_DRV
 */
const std::vector<std::string> RC_SpeedNames = {"20ms","15ms:1","10ms","5ms"};
const std::vector<std::string> PWM_SpeedNames = {"3khz","6khz:1","12khz","23.5khz"};
// Names of SpeedPWM_DRV
const std::vector<std::string> PwmModeNames = {"RC PPM","0%-50%-100% Centered","0-100% PWM/DIR (HB)"};


ClassIdentifier MotorPWM::info = {
		 .name = "PWM" ,
		 .id=2
 };
const ClassIdentifier MotorPWM::getInfo(){
	return info;
}

void MotorPWM::turn(int16_t power){
	if(!active)
		return;

	/*
	 * Generates a classic RC 20ms 1000-2000µs signal
	 * Centered at 1500µs for bidirectional RC ESCs and similiar stuff
	 */
	if(mode == ModePWM_DRV::RC_PWM){
		int32_t pval = power;
		float val = ((pval * 1000)/0x7fff)*tFreq;
		val = clip((1500*tFreq)-val,1000*tFreq, 2000*tFreq);
		setPWM(val);

	/*
	 * Generates a 0-100% PWM signal on CS3
	 * and outputs complementary direction signals on CS1 and CS2
	 * Can be used with cheap halfbridge modules and DC motors
	 */
	}else if(mode == ModePWM_DRV::PWM_DIR){
		if(power < 0){
			HAL_GPIO_WritePin(rightPort, rightPin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(leftPort, leftPin, GPIO_PIN_SET);
		}else{
			HAL_GPIO_WritePin(leftPort, leftPin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(rightPort, rightPin, GPIO_PIN_SET);
		}
		int32_t val = (uint32_t)((abs(power) * period)/0x7fff);
		setPWM(val);

	}else if(mode == ModePWM_DRV::CENTERED_PWM){
		int32_t pval = 0x7fff+power;
		int32_t val = (pval * period)/0xffff;
		setPWM(val);
	}
}

/*
 * Setup the timer for different frequency presets
 */
void MotorPWM::setPwmSpeed(SpeedPWM_DRV spd){
	bool ok = true;
	switch(spd){
	case SpeedPWM_DRV::LOW:
		if(mode == ModePWM_DRV::RC_PWM){
			period = 1900000;  //20ms (20000/95)
			prescaler = 0;
		}else{
			period = 0x7fff;
			prescaler = 0;
		}

	break;
	case SpeedPWM_DRV::MID:
		if(mode == ModePWM_DRV::RC_PWM){
			period = 1410000;//15ms(30000/47)
			prescaler = 0;
		}else{
			period = 0x3fff;
			prescaler = 0;
		}

	break;
	case SpeedPWM_DRV::HIGH:
		if(mode == ModePWM_DRV::RC_PWM){
			period = 940000; //10ms (20000/47)
			prescaler = 0;
		}else{
			period = 0x1fff;
			prescaler = 0;
		}
	break;
	case SpeedPWM_DRV::VERYHIGH:
		if(mode == ModePWM_DRV::RC_PWM){
			period = 460000; //5ms (20000/23)
			prescaler = 0;
		}else{
			period = 0x0fff;
			prescaler = 0;
		}
	break;
	default:
		ok = false;
	}

	if(ok){
		this->pwmspeed = spd;
		tFreq = (float)basefreq/(float)(prescaler+1);
		pwmInitTimer(timer, channel,period,prescaler);
		setPWM_HAL(0, timer, channel, period);
	}
}

/*
 * Updates pwm pulse length
 */
void MotorPWM::setPWM(uint32_t value){

	timer->Instance->CCR3 = value; // Set next CCR for channel 3
}

SpeedPWM_DRV MotorPWM::getPwmSpeed(){
	return this->pwmspeed;
}


MotorPWM::MotorPWM() {
	// Setup PWM timer (timer2!) on CS3 pin
	HAL_GPIO_DeInit(SPI1_SS3_GPIO_Port, SPI1_SS3_Pin);
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = SPI1_SS3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(SPI1_SS3_GPIO_Port, &GPIO_InitStruct);

	restoreFlash();
	//HAL_TIM_Base_Start_IT(timer);
	setPwmSpeed(pwmspeed);
}

void MotorPWM::saveFlash(){
	// 0-3: mode
	// 4-6: speed
	uint16_t var = (uint8_t)this->mode & 0xf;
	var |= ((uint8_t)this->pwmspeed & 0x7) << 4;
	Flash_Write(ADR_PWM_MODE, var);
}
void MotorPWM::restoreFlash(){
	uint16_t var = 0;
	if(Flash_Read(ADR_PWM_MODE, &var)){
		uint8_t m = var & 0xf;
		this->mode = ModePWM_DRV(m);

		uint8_t s = (var >> 4) & 0x7;
		this->pwmspeed = SpeedPWM_DRV(s);
	}
}


MotorPWM::~MotorPWM() {
	HAL_TIM_PWM_Stop(timer, channel);
	HAL_TIM_Base_Stop_IT(timer);
}


void MotorPWM::start(){
	active = true;
}

void MotorPWM::stop(){
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



bool MotorPWM::command(ParsedCommand* cmd,std::string* reply){
	bool result = true;

	if(cmd->cmd == "pwm_mode"){
		if(cmd->type == CMDtype::set){
			this->setMode((ModePWM_DRV)cmd->val);
		}else if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint8_t)this->getMode());
		}else{
			for(uint8_t i = 0; i<PwmModeNames.size();i++){
				*reply+=  PwmModeNames[i]  + ":" + std::to_string(i)+"\n";
			}
		}
	}else if(cmd->cmd == "pwm_speed"){
		if(cmd->type == CMDtype::set){
			this->setPwmSpeed((SpeedPWM_DRV)cmd->val);
		}else if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint8_t)this->getPwmSpeed());
		}else{
			std::vector<std::string> names = PWM_SpeedNames;
			if(this->mode == ModePWM_DRV::RC_PWM){
				names = RC_SpeedNames;
			}else{
				names = PWM_SpeedNames;
			}
			for(uint8_t i = 0; i<names.size();i++){
				*reply+=  names[i]  + ":" + std::to_string(i)+"\n";
			}
		}
	}else{
		result = false; // No valid command
	}

	return result;
}






void pwmInitTimer(TIM_HandleTypeDef* timer,uint32_t channel,uint32_t period,uint32_t prescaler){
	HAL_TIM_PWM_DeInit(timer);
	timer->Init.Prescaler = prescaler;
	timer->Init.CounterMode = TIM_COUNTERMODE_UP;
	timer->Init.Period = period;
	timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	timer->Instance->CCMR1 |= TIM_CCMR1_OC1PE;
	timer->Instance->CR1 = TIM_CR1_ARPE;

	HAL_TIM_PWM_Init(timer);

	TIM_OC_InitTypeDef oc_init;
	oc_init.OCMode = TIM_OCMODE_PWM1;
	oc_init.OCPolarity = TIM_OCPOLARITY_HIGH;
	oc_init.Pulse = 0;
	HAL_TIM_PWM_ConfigChannel(timer, &oc_init, channel);
	HAL_TIM_PWM_Start(timer, channel);

}


/*
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
