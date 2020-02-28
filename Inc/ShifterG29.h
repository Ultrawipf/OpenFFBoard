/*
 * ShifterG29.h
 *
 *  Created on: 27.02.2020
 *      Author: Yannick
 */

#ifndef SHIFTERG29_H_
#define SHIFTERG29_H_
#include "ButtonSource.h"
#include "AdcHandler.h"

class ShifterG29 : public ButtonSource, AdcHandler {

/*
 * Button mapper for Logitech G29 shifters (6 gears + reverse)
 * Connection:
 * X-Axis: A5
 * Y-Axis: A6
 * Reverse Button: D1
 *
 * Invert flag switches between H-mode and sequential mode (90° rotated)
 */

public:
	ShifterG29();
	virtual ~ShifterG29();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	void readButtons(uint32_t* buf);
	uint16_t getBtnNum(); // Amount of readable buttons

	void adcUpd(volatile uint32_t* ADC_BUF);

	void saveFlash();
	void restoreFlash();

	const uint16_t maxButtons = 7;
private:
	volatile uint32_t ADC_BUF[ADC_CHANNELS] = {0};

	const uint8_t x_chan = 5;
	const uint8_t y_chan = 4;

	uint16_t rev_pin = DIN0_Pin;
	GPIO_TypeDef* rev_port = DIN0_GPIO_Port;

	// H-Shifter axis values (Measured for G26)
	const uint16_t X_12 = 1600;
	const uint16_t X_56 = 2500;
	const uint16_t Y_135 = 3200;
	const uint16_t Y_246 = 850;

	uint16_t x_val = 0;
	uint16_t y_val = 0;

	uint8_t gear = 0;
};

#endif /* SHIFTERG29_H_ */
