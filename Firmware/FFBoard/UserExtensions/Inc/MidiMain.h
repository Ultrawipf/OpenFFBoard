/*
 * MidiMain.h
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

// TODO rewrite for tinyusb!
#ifndef MidiMAIN_H_
#define MidiMAIN_H_
#ifdef MIDI
#include <FFBoardMain.h>
#include "cppmain.h"
#include "TMC4671.h"
#include "MotorDriver.h"
#include "TimerHandler.h"
#include "vector"
#include "MidiHandler.h"
#include "thread.hpp"

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
	static bool isCreatable() {return true;};
	ParseStatus command(ParsedCommand* cmd,std::string* reply);
	void usbInit();
	void update();

	void noteOn(uint8_t chan, uint8_t note,uint8_t velocity);
	void noteOff(uint8_t chan, uint8_t note,uint8_t velocity);
	void controlChange(uint8_t chan, uint8_t c, uint8_t val);
	void pitchBend(uint8_t chan, int16_t val);

	void timerElapsed(TIM_HandleTypeDef* htim);

	void play();

	std::unique_ptr<TMC4671> drv;

	uint32_t movementrange = 0x3fff;

private:

	volatile bool updateflag = false;
	float noteToFreq[128] = {0};
	std::vector<MidiNote> notes[16];
	bool active[16] = {false};
	uint32_t power = 4000;

	const uint16_t period = 100;//71;	// Microseconds
	float periodf = period / 1000000.0; // seconds
};

#endif /* MidiMAIN_H_ */
#endif
