/*
 * MidiMain.h
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#ifndef MidiMAIN_H_
#define MidiMAIN_H_

#include <FFBoardMain.h>
#include "cppmain.h"
#include "usbd_desc.h"
#include "TMC4671.h"
#include "MotorDriver.h"
#include "TimerHandler.h"
#include "vector"

extern "C" {
#include "usbd_midi.h"
}

class MidiHandler{
public:
	virtual void noteOn(uint8_t chan, uint8_t note,uint8_t velocity)=0;
	virtual void noteOff(uint8_t chan, uint8_t note,uint8_t velocity)=0;
	virtual void controlChange(uint8_t chan, uint8_t c, uint8_t val)=0;
	virtual void pitchBend(uint8_t chan, int16_t val)=0;
};

struct MidiNote{
	uint8_t note = 0;
	uint8_t volume = 0;
	float counter = 0;
	float pitchbend = 1;
};

class MidiMain: public FFBoardMain, public MidiHandler,TimerHandler {
public:
	MidiMain();
	virtual ~MidiMain();

	TIM_HandleTypeDef* timer_update;

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	ParseStatus command(ParsedCommand* cmd,std::string* reply);
	void usbInit(USBD_HandleTypeDef* hUsbDeviceFS);
	void update();
	void  SOF();

	void noteOn(uint8_t chan, uint8_t note,uint8_t velocity);
	void noteOff(uint8_t chan, uint8_t note,uint8_t velocity);
	void controlChange(uint8_t chan, uint8_t c, uint8_t val);
	void pitchBend(uint8_t chan, int16_t val);

	void timerElapsed(TIM_HandleTypeDef* htim);

	void play();

	TMC4671* drv;

	uint32_t movementrange = 0x3fff;

	static int8_t Midi_Receive(uint8_t *msg, uint32_t len);
	USBD_ClassTypeDef* handles[2];
	USBD_Midi_ItfTypeDef USBD_Midi_fops = {
		Midi_Receive,
	};

private:
	/*
	 * Tries to correct note periods by using the USB SOF.
	 * May cause jitter but better tuning if timing is off due to interrupts or sysclock errors.
	 */
	const bool correctFrequency = true;

	volatile bool updateflag = false;
	float noteToFreq[128] = {0};
	std::vector<MidiNote> notes[16];
	bool active[16] = {false};
	uint32_t power = 4000;

	const uint16_t period = 71;	// Microseconds
	float periodf = period / 1000000.0; // seconds
	float freqErr = 1;
	uint16_t timersSinceSOF = 0;
	uint32_t lastSystick = 0;
};

#endif /* MidiMAIN_H_ */
