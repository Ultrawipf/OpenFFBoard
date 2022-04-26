/*
 * EncoderBissC.h
 *
 *  Created on: 19.12.2020
 *      Author: Leon & Yannick
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
#include "PersistentStorage.h"
#include "semaphore.hpp"

class EncoderBissC: public Encoder, public CommandHandler, public SPIDevice,public PersistentStorage {
public:

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	EncoderBissC();
	virtual ~EncoderBissC();

	EncoderType getType();
	static bool isCreatable();

	int32_t getPos();
	void setPos(int32_t pos);
	uint32_t getCpr();

	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash

	enum class EncoderBissC_commands {bits,cs,speed,rawpos,errors};

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
//	virtual std::string getHelpstring() {
//			return "BissC Encoder: bissCnbBitData, bissCsetSpeed, bissCnbError, bissCgetRawPos\n";
//		};
	void registerCommands();
	void configSPI();
	void acquirePosition();
	void spiRxCompleted(SPIPort* port) override;
	void beginSpiTransfer(SPIPort* port);
	void endSpiTransfer(SPIPort* port);

private:
	int lenghtDataBit = 22;
	int spiSpeed = 3;
	bool waitData = false;

	int32_t pos = 0, posOffset = 0;
	int32_t mtpos = 0;
	bool crc_ok = false;
	const static uint8_t bytes = 8;
	uint8_t spi_buf[bytes] = {0}, decod_buf[bytes] = {0};


	uint8_t POLY = 0x43;
	uint8_t tableCRC6n[64] = {0};
	int32_t numErrors = 0;
	static bool inUse;
	cpp_freertos::BinarySemaphore readSem = cpp_freertos::BinarySemaphore(false);
};

#endif /* ENCODERBISSC_H_ */
