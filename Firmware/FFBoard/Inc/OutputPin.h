/*
 * SPI.h
 *
 *  Created on: 30.12.2020
 *      Author: willson556
 */

#include "stm32f4xx_hal.h"
#ifndef OUTPUTPIN_H_
#define OUTPUTPIN_H_
/// For now this class only works with pre-configured output pins but it could be
/// easily extended to cover input pins as well as expose various configuration API's.
class OutputPin {
protected:
	GPIO_TypeDef *port;
	uint16_t pin;
	//const std::string name;

public:
	OutputPin(GPIO_TypeDef &port, uint16_t pin) // const std::string name = ""
		: port{&port}, pin{pin} {}

	//OutputPin(const OutputPin& p): port{p.port}, pin{p.pin} {};
	void set() const {
		write(true);
	}

	void reset() const {
		write(false);      
	}

	void write(bool state) const {
		HAL_GPIO_WritePin(port, pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}

	bool operator==(const OutputPin& b){
		return(this->port == b.port && this->pin == b.pin);
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
	/**
	 * Sets a pin into output mode in case it was previously reconfigured
	 */
	void configureOutput(uint32_t pull = GPIO_NOPULL,bool opendrain = false, uint32_t speed = GPIO_SPEED_FREQ_LOW){
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = pin;
		GPIO_InitStruct.Mode = opendrain ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = pull;
		GPIO_InitStruct.Speed = speed;
		HAL_GPIO_Init(port, &GPIO_InitStruct);
	}
#pragma GCC diagnostic pop
	//const std::string getName(){return name;}


};
#endif
