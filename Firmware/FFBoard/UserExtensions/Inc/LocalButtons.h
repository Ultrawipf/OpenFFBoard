/*
 * LocalButtons.h
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#ifndef LOCALBUTTONS_H_
#define LOCALBUTTONS_H_

#include <ButtonSource.h>
#include "ChoosableClass.h"
#include "CommandHandler.h"
#include "constants.h"
#include <array>
#include "GPIOPin.h"

class LocalButtons: public ButtonSource,CommandHandler{
	enum class LocalButtons_commands : uint32_t{
		mask,polarity,pins,values,pulse
	};
private:
	uint32_t mask = 0xff; // Can have 16 bits stored
	uint32_t pulsemask = 0;
	static constexpr uint32_t pulseTimeout = 50; // Update only every 50 ms
	uint32_t lastPulseTime = 0;
	void setMask(uint32_t mask);
	bool polarity = false;
	static const std::array<InputPin,BUTTON_PINS> button_pins;
	uint64_t lastOutputs = 0;
	uint64_t lastButtons = 0;
public:
	LocalButtons();
	virtual ~LocalButtons();
	uint8_t readButtons(uint64_t* buf);
	uint8_t getButtonInputs(uint64_t* buf,bool pol);

	const ClassIdentifier getInfo();
	static ClassIdentifier info;
	static bool isCreatable() {return true;};

	static constexpr uint16_t maxButtons{BUTTON_PINS};

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);

	std::string getHelpstring(){return "Digital pin button source";};

	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash

	static inline bool readButton(int button_num) {
		if (button_num >= maxButtons || button_num < 0) {
			return false;
		}
//		return HAL_GPIO_ReadPin(button_ports[button_num], button_pins[button_num]);
		return button_pins[button_num].read();
	}
	const ClassType getClassType() {return ClassType::Buttonsource;};
};

#endif /* LOCALBUTTONS_H_ */
