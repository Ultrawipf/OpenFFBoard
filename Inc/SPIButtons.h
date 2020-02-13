/*
 * ButtonSourceSPI.h
 *
 *  Created on: 11.02.2020
 *      Author: Yannick
 */

#ifndef SPIBUTTONS_H_
#define SPIBUTTONS_H_

#include <ButtonSource.h>
#include "cppmain.h"


class SPI_Buttons: public ButtonSource {
public:
	SPI_Buttons();
	virtual ~SPI_Buttons();
	
	void readButtons(uint8_t* buf ,uint16_t len);
	uint16_t getBtnNum(); // Amount of readable buttons
	void setBtnNum(uint16_t num);

private:
	SPI_HandleTypeDef* spi;
	uint16_t cspin;
	GPIO_TypeDef* csport;

	uint16_t buttons=16;
};

#endif /* SPIBUTTONS_H_ */
