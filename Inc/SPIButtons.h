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
#include "ChoosableClass.h"


class SPI_Buttons: public ButtonSource {
public:
	SPI_Buttons();
	virtual ~SPI_Buttons();
	const ClassIdentifier getInfo();
	static ClassIdentifier info;
	void setConfig(ButtonSourceConfig config);
	
	void readButtons(uint32_t* buf);
	void setInvert(bool invert);

	static int16_t readConfNumButtons();
	static void writeConfNumButtons(int16_t num);
private:
	SPI_HandleTypeDef* spi;
	uint16_t cspin;
	GPIO_TypeDef* csport;
};

#endif /* SPIBUTTONS_H_ */
