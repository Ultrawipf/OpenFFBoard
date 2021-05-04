/*
 * cpp_target_config.h
 *
 *  Created on: 27.12.2020
 *      Author: willson556
 */

#ifndef CPP_TARGET_CONFIG_H_
#define CPP_TARGET_CONFIG_H_

#include "SPI.h"
#include "UART.h"
#include "target_constants.h"

extern SPIPort external_spi;
extern SPIPort motor_spi;

#ifdef UART_PORT_MOTOR
extern UARTPort motor_uart;
#endif

#ifdef UART_PORT_EXT
extern UARTPort external_uart;
#endif

#endif
