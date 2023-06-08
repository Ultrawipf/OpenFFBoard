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

	switch(packet[1]){ // System packet

		case MIDI_STATUS_SYSEX_START:
		{
			sysexState = true;
			otherPacket(packet);
			break;
		}
		case MIDI_STATUS_SYSEX_END:
			sysexState = false;
			otherPacket(packet);
			break;

		//------------- System Common -------------//
//			case MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME:
//			case MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER:
//			case MIDI_STATUS_SYSCOM_SONG_SELECT:
//			// F4, F5 is undefined
//			case MIDI_STATUS_SYSCOM_TUNE_REQUEST:
//				sysexState = false;
//				break;

		//------------- System RealTime  -------------//
		case MIDI_STATUS_SYSREAL_TIMING_CLOCK:
			// tick
			this->midiTick();
			break;
		// 0xF9 is undefined
//			case MIDI_STATUS_SYSREAL_START:
//			case MIDI_STATUS_SYSREAL_CONTINUE:
//			case MIDI_STATUS_SYSREAL_STOP:
//			// 0xFD is undefined
//			case MIDI_STATUS_SYSREAL_ACTIVE_SENSING:
//			case MIDI_STATUS_SYSREAL_SYSTEM_RESET:
//				sysexState = false;
//				break;

		default:

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
				this->programChange(chan,b1);
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
				otherPacket(packet);
				break;
		}
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
/**
 * Sent 24 times per quarter note
 */
void MidiHandler::midiTick(){

}
void MidiHandler::programChange(uint8_t chan, uint8_t val){

}
void MidiHandler::otherPacket(uint8_t packet[4]){

}

#endif
