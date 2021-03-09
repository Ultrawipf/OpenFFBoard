/*
 * MidiHandler.cpp
 *
 *  Created on: 23.02.2021
 *      Author: Yannick
 */
#include "MidiHandler.h"
#ifdef MIDI

uint8_t MidiHandler::buf[] = {0};

MidiHandler::MidiHandler() {
	extern MidiHandler* midihandler; // Global pointer
	midihandler = this;

}

MidiHandler::~MidiHandler() {
	extern MidiHandler* midihandler; // Global pointer
	midihandler = nullptr;
}

void MidiHandler::midiRx(uint8_t itf,uint8_t packet[4]){

	pulseSysLed();
	uint8_t chan = packet[1] & 0xf;
	uint8_t packettype = packet[1] & 0xf0;
	uint8_t b1 =  packet[2];
	uint8_t b2 =  packet[3];

	switch (packettype) {
	case 0x80:
		// Noteoff
		this->noteOff(chan, b1, b2);
		break;
	case 0x90:
		// Noteon
		if(b2 == 0){
			this->noteOff(chan, b1, b2);
		}else{
			this->noteOn(chan, b1, b2);
		}
		break;
	case 0xB0:
		//cc
		this->controlChange(chan, b1,b2);
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
		this->pitchBend(chan, pb-8192);
		break;
	}
	default:
		break;
	}
}

void MidiHandler::noteOn(uint8_t chan, uint8_t note,uint8_t velocity){

}
void MidiHandler::noteOff(uint8_t chan, uint8_t note,uint8_t velocity){

}
void MidiHandler::controlChange(uint8_t chan, uint8_t c, uint8_t val){

}
void MidiHandler::pitchBend(uint8_t chan, int16_t val){

}

#endif
