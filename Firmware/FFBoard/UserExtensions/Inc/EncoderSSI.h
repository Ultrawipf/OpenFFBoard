/*
 * EncoderSSI.h
 *
 *  Created on: 22.02.23
 *      Author: Yannick
 */

#ifndef ENCODERSSI_H_
#define ENCODERSSI_H_

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
#include "array"

class EncoderSSI: public Encoder, public SPIDevice , public CommandHandler,public PersistentStorage {
public:

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	EncoderSSI();
	virtual ~EncoderSSI();

	EncoderType getEncoderType();
	static bool isCreatable();
	bool updateFrame();
	int32_t getPos();
	int32_t getPosAbs();
	void setPos(int32_t pos);
	uint32_t getCpr();

	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash
	std::string getHelpstring(){return "SPI SSI Encoder\n";}

	enum class EncoderSSI_commands {bits,cs,speed,errors,mode};
	enum class EncoderSSI_modes : uint8_t{rawmsb,AMT23}; // 15 max modes
	static constexpr std::array<const char*,2> mode_names = {"Raw","AMT23"};

	template<size_t N>
	std::string printModes(const std::array<const char*,N>& names){
		std::string reply;
		for(uint8_t i = 0; i<names.size();i++){
			reply+=  std::string(names[i])  + ":" + std::to_string(i)+"\n";
		}
		return reply;
	}

	std::string printSpeeds();

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);

	void registerCommands();
	void configSPI();

	void spiRxCompleted(SPIPort* port) override;
	void beginSpiTransfer(SPIPort* port);
	void endSpiTransfer(SPIPort* port);
	void setMode(EncoderSSI_modes mode);

private:
	int lenghtDataBit = 22;
	int spiSpeed = 0;
	uint32_t errors = 0;
	EncoderSSI_modes mode;

	uint32_t lastUpdateTick = 0;

	int32_t pos = 0, posOffset = 0,lastPos = 0,newPos = 0;
	int32_t mtpos = 0;

	const static uint8_t bytes = 8;
	uint8_t spi_buf[bytes] = {0};
	uint8_t transferlen = bytes;

	static bool inUse;

	bool waitData = false;

};

#endif
