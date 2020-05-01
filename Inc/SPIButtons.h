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
#include "CommandHandler.h"

struct ButtonSourceConfig{
	uint8_t numButtons = 32;
	bool cutRight = false; // if num buttons to read are not byte aligned specify where to shift
	bool invert = false;
	uint8_t extraOptions = 0; // Save anything extra here like special flags
};

class SPI_Buttons: public ButtonSource,CommandHandler {
public:
	SPI_Buttons();
	virtual ~SPI_Buttons();
	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	void readButtons(uint32_t* buf);

	bool command(ParsedCommand* cmd,std::string* reply);

	void saveFlash();
	void restoreFlash();

	const uint8_t maxButtons = 32;

private:
	static ButtonSourceConfig decodeIntToConf(uint16_t val);
	static uint16_t encodeConfToInt(ButtonSourceConfig* c);

	void setConfig(ButtonSourceConfig config);
	virtual ButtonSourceConfig* getConfig();
	void process(uint32_t* buf);
	SPI_HandleTypeDef* spi;
	uint16_t cspin;
	GPIO_TypeDef* csport;
	uint8_t bytes = 4;
	uint16_t mask = 0xff;
	uint8_t offset = 0;

	ButtonSourceConfig conf;
};

#endif /* SPIBUTTONS_H_ */
