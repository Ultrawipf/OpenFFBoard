/*
 * MidiMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include "target_constants.h"
#ifdef MIDI
#include <MidiMain.h>

#include "math.h"
#include "ledEffects.h"
#include "USBdevice.h"
#include "cmsis_os2.h"

ClassIdentifier MidiMain::info = {
		 .name = "MIDI" ,
		 .id=64,
		 .unique = '0',
		 .hidden=false //Set false to list
 };

const ClassIdentifier MidiMain::getInfo(){
	return info;
}


MidiMain::MidiMain(){
	// Generate notes
	for(uint8_t i = 0;i<128;i++){
		float f = std::pow(2, (i - 69) / 12.0) * 440.0;
		this->noteToFreq[i] = f;
	}

	// Setup timer
	extern TIM_HandleTypeDef TIM_USER;
	this->timer_update = &TIM_USER; // Timer setup with prescaler of sysclock
	this->timer_update->Instance->ARR = period;
	this->timer_update->Instance->PSC = (SystemCoreClock / 1000000)-1;
	this->timer_update->Instance->CR1 = 1;
	HAL_TIM_Base_Start_IT(this->timer_update);

	// Setup one TMC for first channel
 	this->drv = new TMC4671();
	TMC4671Limits limits;
	drv->setLimits(limits);
	drv->setAddress(1);
	//drv->setMotorType(MotorType::STEPPER, 50);
	if(drv->conf.motconf.motor_type == MotorType::NONE){
		pulseErrLed();
	}
	drv->setPhiEtype(PhiE::ext);
	drv->setUdUq(0, 0);
	drv->allowSlowSPI = false; // Force higher speed
	drv->initialize();
	drv->setPhiEtype(PhiE::ext);
	drv->setMotionMode(MotionMode::uqudext,true);

	if(!drv->initialized){
		pulseErrLed();
	}
	//this->Start(); // We do not start the driver thread

}

MidiMain::~MidiMain() {

}


void MidiMain::update(){
	osDelay(50); // Slow down main thread
}

void MidiMain::timerElapsed(TIM_HandleTypeDef* htim){
	if(htim == this->timer_update){
		play();
	}
}

void MidiMain::play(){
	// Take only first channel for now and last note...
	uint8_t chan = 0;
	if(notes[chan].empty()){
		if(active[chan]){
			active[chan] = false;
			// stop driver
			drv->setUdUq(0, 0);
		}
	}else{
		if(!active[chan]){
			drv->setUdUq(this->power,0);
			active[chan] = true;
		}

		MidiNote* note = &notes[chan].back();
		float freq = noteToFreq[note->note];
		// Speed up period counter instead of changing frequency to prevent phase jumps
		float time = periodf;//(abs((uint16_t)this->timer_update->Instance->CNT - lastSystick)%period) / 1000000.0;
		//lastSystick = this->timer_update->Instance->CNT;
		note->counter += time * note->pitchbend;

		float volume = note->volume / 127.0f;
		float p = (note->counter*freq);
		float sine =  sinf(M_PI*p) * volume;
		int16_t val = (sine * movementrange); // 180° phase range

		drv->setPhiE_ext(val); // Wobble motor
	}
}

void MidiMain::noteOn(uint8_t chan, uint8_t note,uint8_t velocity){
	// If note already present remove
	noteOff(chan,note,velocity);

	MidiNote midinote;
	midinote.note = note;
	midinote.counter = 0;
	midinote.volume = velocity;
	notes[chan].push_back(midinote);
}

void MidiMain::noteOff(uint8_t chan, uint8_t note,uint8_t velocity){
	for(auto it = notes[chan].begin(); it!=notes[chan].end(); ++it){
		if(it->note == note){
			notes[chan].erase(it);
			break;
		}
	}

}

void MidiMain::controlChange(uint8_t chan, uint8_t c, uint8_t val){
	if(c == 120 || c == 121 || c == 123){
		notes[chan].clear();
	}
}
void MidiMain::pitchBend(uint8_t chan, int16_t val){
	float pb = std::pow(2.0f, (((float)val/8192.0f)));
	//printf("PB: %d, %f\n",val,pb);
	for(auto it = notes[chan].begin(); it!=notes[chan].end(); ++it){
		it->pitchbend =  pb;
	}
}

ParseStatus MidiMain::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus flag = ParseStatus::OK; // Valid command found
	if(cmd->cmd == "power"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(power);
		}else if(cmd->type == CMDtype::set){
			this->power = cmd->val;
		}
	}else if(cmd->cmd == "range"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(movementrange);
		}else if(cmd->type == CMDtype::set){
			this->movementrange = cmd->val;
		}
	}else{
		flag=drv->command(cmd, reply);
	}
	return flag;
}



void MidiMain::usbInit(){
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_midi_conf,&usb_ffboard_strings_default);
	usbdev->registerUsb();
}

#endif
