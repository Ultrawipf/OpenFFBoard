/*
 * cpp_target_config.h
 *
 *  Created on: 27.12.2020
 *      Author: willson556
 */

#ifndef CPP_TARGET_CONFIG_H_
#define CPP_TARGET_CONFIG_H_

#include "SPI.h"

extern SPIPort external_spi;

const std::vector<OutputPin*>& getExternalSPI_CSPins();

inline OutputPin& getExternalSPI_CSPin(unsigned pinNumber) {
	auto csPins{getExternalSPI_CSPins()};
	if (pinNumber > csPins.size()) {
		return *csPins[0];
	}

	return *csPins[pinNumber - 1];
}

#endif
