/*
 * cpp_target_config.h
 *
 *  Created on: 27.12.2020
 *      Author: willson556
 */

#ifndef CPP_TARGET_CONFIG_H_
#define CPP_TARGET_CONFIG_H_

#include "SPI.h"
#include "MotorPWM.h"
#include "UART.h"
#include "target_constants.h"
#include "CAN.h"
#include "I2C.h"

extern SPIPort external_spi;
extern SPIPort motor_spi;
extern SPIPort ext3_spi;

#ifdef UART_PORT_MOTOR
extern UARTPort motor_uart;
#endif

#ifdef UART_PORT_EXT
extern UARTPort external_uart;
#endif

#ifdef CANBUS
extern CANPort& canport;
#endif

#ifdef PWMDRIVER
extern const PWMConfig pwmTimerConfig;
#endif

#ifdef I2C_PORT
extern I2CPort i2cport;
#endif

#ifdef DEBUGPIN
extern const OutputPin debugpin;
#endif

#ifdef GPIO_MOTOR
extern const OutputPin gpMotor;
#endif

#endif
