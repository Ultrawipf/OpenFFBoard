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

#endif