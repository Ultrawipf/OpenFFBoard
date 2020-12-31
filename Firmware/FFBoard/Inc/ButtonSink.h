/*
 * ButtonSink.h
 *
 *  Created on: 29.12.2020
 *      Author: willson556
 */

#ifndef BUTTONSINK_H_
#define BUTTONSINK_H_

#include <cstdint>
#include <vector>

class ButtonSink {
public:
	ButtonSink();
	virtual ~ButtonSink();

	virtual void updateButtonState(uint32_t* buf, uint16_t numButtons) = 0;

	static std::vector<ButtonSink*>& getButtonSinks() {
		static std::vector<ButtonSink*> buttonSinks{};
		return buttonSinks;
	}
};

bool getButtonState(uint32_t* buf, uint16_t numButtons, uint16_t buttonIndex);

#endif /* BUTTONSOURCE_H_ */
