/*
 * GPIOPin.h
 *
 *  Created on: 30.12.2020
 *      Author: willson556
 */
#include "cppmain.h"

#ifndef GPIOPIN_H_
#define GPIOPIN_H_
/// For now this class only works with pre-configured output pins but it could be
/// easily extended to cover input pins as well as expose various configuration API's.

class GpioPin{
protected:
	GPIO_TypeDef *port;
	uint16_t pin;
public:
	GpioPin(GPIO_TypeDef &port, uint16_t pin) // const std::string name = ""
		: port{&port}, pin{pin} {}


	bool operator==(const GpioPin& b){
		return(this->port == b.port && this->pin == b.pin);
	}
	const GPIO_TypeDef* getPort() const {return port;}
	uint16_t getPin() const {return pin;}

};


class OutputPin : public GpioPin {
public:
	OutputPin(GPIO_TypeDef &port, uint16_t pin) // const std::string name = ""
		: GpioPin(port,pin) {}

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

	//const std::string getName(){return name;}

};


class InputPin : public GpioPin {
public:
	InputPin(GPIO_TypeDef &port, uint16_t pin) // const std::string name = ""
		: GpioPin(port,pin) {}
	InputPin(GPIO_TypeDef* port, uint16_t pin) // const std::string name = ""
		: GpioPin(*port,pin) {}

	//OutputPin(const OutputPin& p): port{p.port}, pin{p.pin} {};
	bool read() const{
		return HAL_GPIO_ReadPin(port,pin);
	}


	//const std::string getName(){return name;}

};
#endif
