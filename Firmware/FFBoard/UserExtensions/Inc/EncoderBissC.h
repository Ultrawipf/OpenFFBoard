/*
 * EncoderDummy.h
 *
 *  Created on: 19.12.2020
 *      Author: Leon
 */

#ifndef ENCODERBISSC_H_
#define ENCODERBISSC_H_

#include "cppmain.h"
#include "constants.h"
#include "ChoosableClass.h"
#include "CommandHandler.h"
#include "SPI.h"
#include "Encoder.h"
#include "cpp_target_config.h"
#include <math.h>

class EncoderBissC: public Encoder, public CommandHandler, public SPIDevice {
public:

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	EncoderBissC();
	virtual ~EncoderBissC();

	EncoderType getType();

	int32_t getPos();
	void setPos(int32_t pos);
	uint32_t getCpr();

	ParseStatus command(ParsedCommand* cmd,std::string* reply);

	void acquirePosition();
	void spiRxCompleted(SPIPort* port) override;


private:
	bool waitData = false;

	int32_t pos = 0;
	int32_t mtpos = 0;
	const static uint8_t bytes = 8;
	uint8_t spi_buf[bytes] = {0}, decod_buf[bytes] = {0};


	uint8_t POLY = 0x43;
	uint8_t tableCRC6n[64] = {0};
	int32_t numErrors = 0;
};

#endif /* ENCODERBISSC_H_ */
