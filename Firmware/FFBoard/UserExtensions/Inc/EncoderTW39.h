/*
 * EncoderTW39.h
 *
 *  Created on: Oct 4, 2026
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_INC_ENCODERTW39_H_
#define USEREXTENSIONS_INC_ENCODERTW39_H_
#include "constants.h"
#ifdef TW39ENCODER
#include "SPI.h"
#include "cpp_target_config.h"
#include "Encoder.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include "thread.hpp"
#include "mutex.hpp"
#include "array"

class EncoderTW39: public Encoder, public SPIDevice, public PersistentStorage, public CommandHandler,cpp_freertos::Thread{
	enum class EncoderTW39_commands : uint32_t{
		cspin,speed,pos,errors,chip,serial,status,temp,outmode,ssi,abzres,abzcfg,uvwpairs,calib,save,reg,cmd
	};

	// Register addresses
	enum class EncoderTW39_reg : uint16_t{
		MAIN_CFG = 0x0000,
		ABZ_CFG = 0x0008,
		ABZ_RES_L = 0x000A,
		ABZ_RES_H = 0x000C,
		UVW_CFG = 0x0012,
		BISS_CFG0 = 0x0018,
		BISS_KEY = 0x0042,
		TEST = 0x0054,
		STAT_VAL = 0x010E,
		STAT_LATCH = 0x0110,
		STAT_FATAL = 0x0112,
		STAT_START = 0x0114,
		COMMAND = 0x4000,
		SERIAL_L = 0x403C,
		SERIAL_H = 0x403E,
		T_NOW = 0x4068,
		CHIP_ID = 0xE000,
		CHIP_REV = 0xE002
	};

	// Values of the command register
	enum class EncoderTW39_chipcmd : uint8_t{
		idle = 0x00,
		stop = 0x02,
		restart = 0x04,
		preset = 0x05,
		clearStatus = 0x06,
		writeEeprom = 0x0C,
		autocal = 0x10
	};

	const std::array<float,3> spispeeds = {10e6,5e6,2.5e6}; // Target speeds. Must double each entry
public:
	EncoderTW39();
	virtual ~EncoderTW39();

	static bool inUse;
	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	static bool isCreatable() {return ENCODER_SPI_PORT.getFreeCsPins().size() > 0 && !inUse;};

	EncoderType getEncoderType(){return EncoderType::absolute;};
	void Run();

	void restoreFlash() override;
	void saveFlash() override;

	int32_t getPos() override;
	uint32_t getCpr() override; // Encoder counts per rotation

	int32_t getPosAbs() override;

	void setPos(int32_t pos);

	void initSPI();
	void updateAngleStatus();
	bool updateAngleStatusCb();

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	std::string getHelpstring(){return "IC-Haus TW39 SPI Encoder\n";}
	void setCsPin(uint8_t cspin);

	void setSpiSpeed(uint8_t preset);

	bool readChipInfo();
	uint16_t readReg(EncoderTW39_reg reg){return readReg((uint16_t)reg);}
	uint16_t readReg(uint16_t addr);
	void writeReg(EncoderTW39_reg reg,uint16_t data){writeReg((uint16_t)reg,data);}
	void writeReg(uint16_t addr,uint16_t data);
	void writeRegBits(EncoderTW39_reg reg,uint16_t mask,uint16_t data);
	void sendChipCommand(EncoderTW39_chipcmd cmd);
	uint8_t getChipCommand();

	void setCalibration(bool enable);
	bool saveEeprom();

private:
	void spiTxRxCompleted(SPIPort* port);
	void spiRequestError(SPIPort* port);

	static const uint8_t packetLen = 8; // Always 64 bit packets
	static const uint8_t angleBits = 26; // Bits of the angle in a position packet
	static const uint8_t posBits = 24; // Resolution reported to the ffboard. Max 26
	static const uint16_t chipIdTW39 = 0x001D;
	static const uint16_t bissKeyUnlock = 0xB4; // Unlocks protected commands
	static const uint32_t eepromTimeout = 1500; // ms. Eeprom write takes up to 1s

	int32_t lastAngleInt = 0;
	int32_t curAngleInt = 0;
	int32_t curPos = 0;
	int32_t rotations = 0;
	int32_t offset = 0;
	uint8_t cspin = 0;
	bool updateInProgress = false;
	volatile bool transferError = false;
	uint32_t errors = 0;
	uint8_t spiStatus = 0; // Status bits of last position packet

	bool connected = false; // Chip id matched
	uint16_t chipId = 0;
	uint16_t chipRev = 0;
	uint32_t serial = 0;
	uint16_t keyBeforeCalib = 0;

	const uint8_t txbuf[packetLen] = {0x10,0,0,0,0,0,0,0}; // Position read. rm=4, wm=0
	uint8_t rxbuf[packetLen] = {0};
	uint8_t rxbuf_t[packetLen] = {0};
	cpp_freertos::BinarySemaphore requestNewDataSem = cpp_freertos::BinarySemaphore(false);
	cpp_freertos::BinarySemaphore waitForUpdateSem = cpp_freertos::BinarySemaphore(false);
	cpp_freertos::MutexStandard transferMutex; // Prevents position updates between packets of a register access

	uint8_t spiSpeedPreset = 0;
	static const uint32_t waitThresh = 2; // If last sample older than x ms use wait semaphore. Else skip and use last value to speed up processing
	uint32_t lastUpdateTick = 0;
};

#endif
#endif /* USEREXTENSIONS_INC_ENCODERTW39_H_ */
