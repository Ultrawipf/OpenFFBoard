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
#include "SpiHandler.h"

// TODO interrupts

// Set this to 64, 128 or 256 to slow down SPI if unstable.
#define SPIBUTTONS_SPEED SPI_BAUDRATEPRESCALER_64

enum class SPI_BtnMode : uint8_t {TM=0,PISOSR=1};


struct ButtonSourceConfig{
	uint8_t numButtons = 8;
	bool cutRight = false; // if num buttons to read are not byte aligned specify where to shift
	bool invert = false;
	SPI_BtnMode mode=SPI_BtnMode::TM; // Mode preset
	bool cspol = false; // Set by preset
};


class SPI_Buttons: public ButtonSource,public CommandHandler,public SpiHandler {
public:
	const std::vector<std::string> mode_names = {"Thrustmaster/HEF4021BT","Shift register (74HC165)"};

	SPI_Buttons();
	virtual ~SPI_Buttons();
	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	void readButtons(uint32_t* buf);

	ParseStatus command(ParsedCommand* cmd,std::string* reply);

	void saveFlash();
	void restoreFlash();

	const uint8_t maxButtons = 32;
	void printModes(std::string* reply);

	void setMode(SPI_BtnMode mode);
	void initSPI();

	void SpiRxCplt(SPI_HandleTypeDef *hspi);

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

	uint8_t spi_buf[4] = {0};
	volatile bool spibusy = false;
};

#endif /* SPIBUTTONS_H_ */
