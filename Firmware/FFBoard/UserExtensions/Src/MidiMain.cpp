/*
 * MidiMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include <MidiMain.h>
#include "usbd_desc.h"
#include "usbd_composite.h"
#include "math.h"
#include "ledEffects.h"

ClassIdentifier MidiMain::info = {
		 .name = "MIDI" ,
		 .id=64,
		 .hidden=false //Set false to list
 };

const ClassIdentifier MidiMain::getInfo(){
	return info;
}


MidiMain::MidiMain() {
	// Generate notes
	for(uint8_t i = 0;i<128;i++){
		float f = std::pow(2, (i - 69) / 12.0) * 440.0;
		this->noteToFreq[i] = f;
	}

	// Setup timer
	extern TIM_HandleTypeDef htim4;
	this->timer_update = &htim4; // Timer setup with prescaler of sysclock
	this->timer_update->Instance->ARR = period;
	this->timer_update->Instance->PSC = (SystemCoreClock / 1000000)-1;
	this->timer_update->Instance->CR1 = 1;
	HAL_TIM_Base_Start_IT(this->timer_update);

	// Setup one TMC for first channel
 	this->drv = new TMC4671();
	TMC4671Limits limits;
	drv->setLimits(limits);
	drv->setAddress(1);
	drv->restoreFlash(); // load motor type
	//drv->setMotorType(MotorType::STEPPER, 50);
	if(drv->conf.motconf.motor_type == MotorType::NONE){
		pulseErrLed();
		printf(">Please select a mottype\n");
	}
	drv->setPhiEtype(PhiE::ext);
	drv->setUdUq(0, 0);
	drv->allowSlowSPI = false; // Force higher speed
	drv->initialize();
	drv->setPhiEtype(PhiE::ext);
	drv->setMotionMode(MotionMode::uqudext);

	if(!drv->initialized){
		pulseErrLed();
	}
}

MidiMain::~MidiMain() {

}

void MidiMain::update(){
	if(updateflag){
		updateflag = false;
		timersSinceSOF++;
		play();
	}
}

void MidiMain::timerElapsed(TIM_HandleTypeDef* htim){
	if(htim == this->timer_update){
		updateflag = true;
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

		note->counter += periodf * note->pitchbend;

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
	}else if(cmd->cmd == "err"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(freqErr);
		}
	}else{
		flag=drv->command(cmd, reply);
	}
	return flag;
}

// Static callback
int8_t MidiMain::Midi_Receive(uint8_t *msg, uint32_t len) {
	extern FFBoardMain* mainclass;
	MidiMain* midi_p = static_cast<MidiMain*>(mainclass);
	pulseSysLed();
	uint8_t chan = msg[1] & 0xf;
	uint8_t msgtype = msg[1] & 0xf0;
	uint8_t b1 =  msg[2];
	uint8_t b2 =  msg[3];
	//MIDI_DataTx(msg,len); //TEST

	switch (msgtype) {
	case 0x80:
		// Noteoff
		midi_p->noteOff(chan, b1, b2);
		break;
	case 0x90:
		// Noteon
		if(b2 == 0){
			midi_p->noteOff(chan, b1, b2);
		}else{
			midi_p->noteOn(chan, b1, b2);
		}
		break;
	case 0xB0:
		//cc
		midi_p->controlChange(chan, b1,b2);
		break;
	case 0xC0:
		//pc
		break;
	case 0xD0:

		break;
	case 0xE0:
	{
		//pb
		int16_t pb = (b1 & 0x7f) | ((b2 & 0x7f) << 7);
		midi_p->pitchBend(chan, pb-8192);
		break;
	}
	default:
		break;
	}

	return 0;
}

void MidiMain::usbInit(USBD_HandleTypeDef* hUsbDeviceFS){
	handles[CDC_IDX] = &USBD_CDC;
	handles[1] = &USBD_Midi_ClassDriver;

	// Base Descriptor
	USB_ConfigDescType base_desc = {
		/*Configuration Descriptor*/
		0x09,   /* bLength: Configuration Descriptor size */
		USB_DESC_TYPE_CONFIGURATION,      /* bDescriptorType: Configuration */
		0x00,                /* wTotalLength:no of returned bytes. Is set later by composite */
		0x04,   /* bNumInterfaces */
		0x01,   /* bConfigurationValue: Configuration value */
		0x02,   /* iConfiguration: Index of string descriptor describing the configuration */
		0xC0,   /* bmAttributes: self powered */
		0x32,   /* MaxPower 100 mA */

	};

	USBD_Init(hUsbDeviceFS, &FS_Desc_Composite, DEVICE_FS);

	// Add descriptors and class functions to composite device
	USBD_Composite_Set_Classes(handles,2,&base_desc);

	// Define endpoints

	//MIDI
	USBD_Composite_EPIN_To_Class(MIDI_IN_EP, 1);
	USBD_Composite_EPOUT_To_Class(MIDI_OUT_EP, 1);
	USBD_Composite_InterfaceToClass(MIDI_INTERFACE_A,1);
	USBD_Composite_InterfaceToClass(MIDI_INTERFACE_B,1);

	// CDC
	USBD_Composite_EPIN_To_Class(CDC_CMD_EP, CDC_IDX);
	USBD_Composite_EPIN_To_Class(CDC_IN_EP, CDC_IDX);
	USBD_Composite_EPIN_To_Class(CDC_OUT_EP, CDC_IDX);

	USBD_Composite_InterfaceToClass(CDC_INTERFACE,CDC_IDX);
	USBD_Composite_InterfaceToClass(CDC_INTERFACE_DATA,CDC_IDX);

	USBD_RegisterClass(hUsbDeviceFS, &USBD_Composite);

	USBD_CDC_RegisterInterface(hUsbDeviceFS, &USBD_Interface_fops_FS);

	USBD_Midi_RegisterInterface(hUsbDeviceFS, &USBD_Midi_fops);
	USBD_Start(hUsbDeviceFS);
}

void MidiMain::SOF(){
	// Correct for timer and clock errors by using USB 1khz SOF frames
	if(correctFrequency){
		if(timersSinceSOF==0){
			return;
		}

		freqErr = (freqErr + (1000.0f/ (timersSinceSOF * period))) / 2.0f;
		if(abs(freqErr-1) < 0.4) // Filter large errors
			periodf = (period / 1000000.0f) * freqErr;
		timersSinceSOF = 0;
	}
}
