/*
 * ButtonSink.cpp
 *
 *  Created on: 29.12.2020
 *      Author: willson556
 */

#include <limits>

#include "ButtonSink.h"
#include "global_callbacks.h"

ButtonSink::ButtonSink() {
	addCallbackHandler(&getButtonSinks(), this);
}

ButtonSink::~ButtonSink() {
	removeCallbackHandler(&getButtonSinks(), this);
}

bool getButtonState(uint32_t* buf, uint16_t numButtons, uint16_t buttonIndex) {
	if (buttonIndex > numButtons) {
		return false;
	}

	auto btnIndex{buttonIndex - 1};
	size_t bufferIndex{0};
	while (btnIndex > std::numeric_limits<uint32_t>::digits) {
		btnIndex -= std::numeric_limits<uint32_t>::digits;
		++bufferIndex;
	}

	return buf[bufferIndex] & (1 << btnIndex);
}
