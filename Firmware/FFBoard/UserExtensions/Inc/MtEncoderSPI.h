/*
 * MtEncoderSPI.h
 *
 *  Created on: 02.11.2021
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_MTENCODERSPI_H_
#define USEREXTENSIONS_SRC_MTENCODERSPI_H_
#include "constants.h"
#ifdef MTENCODERSPI
#include "SPI.h"
#include "cpp_target_config.h"
#include "Encoder.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include "thread.hpp"

#define MAGNTEK_READ 0x80

class MtEncoderSPI: public Encoder, public SPIDevice, public PersistentStorage, public CommandHandler,cpp_freertos::Thread{
	enum class MtEncoderSPI_commands : uint32_t{
		cspin,pos,errors
	};
public:
	MtEncoderSPI();
	virtual ~MtEncoderSPI();

	static bool inUse;
	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	static bool isCreatable() {return ext3_spi.getFreeCsPins().size() > 0 && !inUse;};

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
	void setCsPin(uint8_t cspin);

	//bool useDMA = false; // if true uses DMA for angle updates instead of polling SPI. TODO when used with tmc external encoder using DMA will hang the interrupt randomly

private:
	uint8_t readSpi(uint8_t addr);
	void writeSpi(uint8_t addr,uint8_t data);
	void spiTxRxCompleted(SPIPort* port);


	bool nomag = false; // Magnet lost in last report
	bool overspeed = false; // Overspeed flag set in last report
	int32_t lastAngleInt = 0;
	int32_t curAngleInt = 0;
	int32_t curPos = 0;
	int32_t rotations = 0;
	int32_t offset = 0;
	uint8_t cspin = 0;
	bool updateInProgress = false;
	uint32_t errors = 0;


	uint8_t txbuf[4] = {0x03 | MAGNTEK_READ,0,0,0};
	uint8_t rxbuf[4] = {0,0,0,0};
	uint8_t rxbuf_t[4] = {0,0,0,0};
	cpp_freertos::BinarySemaphore requestNewDataSem = cpp_freertos::BinarySemaphore(false);
	cpp_freertos::BinarySemaphore waitForUpdateSem = cpp_freertos::BinarySemaphore(false);
};

#endif /* USEREXTENSIONS_SRC_MTENCODERSPI_H_ */
#endif
