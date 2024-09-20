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
#include "array"

class EncoderBissC: public Encoder, public SPIDevice , public CommandHandler,cpp_freertos::Thread,public PersistentStorage {
public:

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	EncoderBissC();
	virtual ~EncoderBissC();

	EncoderType getEncoderType();
	static bool isCreatable();
	bool updateFrame();
	int32_t getPos();
	int32_t getPosAbs();
	void setPos(int32_t pos);
	uint32_t getCpr();

	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash

	void Run();

	enum class EncoderBissC_commands {bits,cs,speed,errors,direction};

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);

	void registerCommands();
	void configSPI();
	void acquirePosition();
	void spiRxCompleted(SPIPort* port) override;
	void beginSpiTransfer(SPIPort* port);
	void endSpiTransfer(SPIPort* port);

private:
	static const bool useWaitSem = true; // Wait until data is processed. otherwise returns last value
	static const uint32_t waitThresh = 2; // If last sample older than x ms use wait semaphore. Else skip and use last value to speed up processing
	int lenghtDataBit = 22;
	int spiSpeed = 3;
	bool waitData = false;
	bool invertDirection = true; // Most biss-c encoders count UP clockwise while standard motor direction is usually CCW

	uint32_t lastUpdateTick = 0;

	int32_t pos = 0, posOffset = 0,lastPos = 0,newPos = 0;
	int32_t mtpos = 0;
	//bool crc_ok = false;
	const static uint8_t bytes = 8; // Maybe use higher length to allow higher speeds because the fixed length start timeout wastes more bits at higher rates
	uint8_t spi_buf[bytes] = {0};
	uint32_t decod_buf[bytes/4] = {0};


	const uint8_t POLY = 0x43;
	static std::array<uint8_t,64> tableCRC6n;
	int32_t numErrors = 0;
	static bool inUse;
	cpp_freertos::BinarySemaphore requestNewDataSem = cpp_freertos::BinarySemaphore(false);
	cpp_freertos::BinarySemaphore waitForUpdateSem = cpp_freertos::BinarySemaphore(false);
};

#endif /* ENCODERBISSC_H_ */
