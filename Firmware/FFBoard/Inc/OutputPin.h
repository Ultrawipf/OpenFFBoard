/*
 * SPI.h
 *
 *  Created on: 30.12.2020
 *      Author: willson556
 */

#include "stm32f4xx_hal.h"


/// For now this class only works with pre-configured output pins but it could be
/// easily extended to cover input pins as well as expose various configuration API's.
class OutputPin {
public:
	OutputPin(GPIO_TypeDef &port, uint16_t pin)
		: port{&port}, pin{pin} {}

	void set() const {
		write(true);
	}

	void reset() const {
		write(false);      
	}

	void write(bool state) const {
		HAL_GPIO_WritePin(port, pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}
private:
	GPIO_TypeDef *port;
	uint16_t pin;
};