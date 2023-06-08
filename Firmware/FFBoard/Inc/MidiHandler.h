/*
 * MidiHandler.h
 *
 *  Created on: 23.02.2021
 *      Author: Yannick
 */

#ifndef SRC_MIDIHANDLER_H_
#define SRC_MIDIHANDLER_H_
#include "target_constants.h"
#ifdef MIDI
#include "cppmain.h"

class MidiHandler {
public:
	MidiHandler();
	virtual ~MidiHandler();
	virtual void midiRx(uint8_t itf,uint8_t packet[4]);
	virtual void noteOn(uint8_t chan, uint8_t note,uint8_t velocity)=0;
	virtual void noteOff(uint8_t chan, uint8_t note,uint8_t velocity)=0;
	virtual void controlChange(uint8_t chan, uint8_t c, uint8_t val);
	virtual void programChange(uint8_t chan, uint8_t val);
	virtual void pitchBend(uint8_t chan, int16_t val);
	virtual void midiTick();
	virtual void otherPacket(uint8_t packet[4]);

	static uint8_t buf[4];
protected:
	bool sysexState = false;
};

#endif /* SRC_MIDIHANDLER_H_ */
#endif
