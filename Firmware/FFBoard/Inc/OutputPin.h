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

	//const std::string getName(){return name;}


};
#endif
