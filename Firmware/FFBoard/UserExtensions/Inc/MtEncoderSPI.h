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

#define MAGNTEK_READ 0x80

class MtEncoderSPI: public Encoder, public SPIDevice, public PersistentStorage, public CommandHandler{
	enum class MtEncoderSPI_commands : uint32_t{
		cspin,pos
	};
public:
	MtEncoderSPI();
	virtual ~MtEncoderSPI();

	static bool inUse;
	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	static bool isCreatable() {return ext3_spi.getFreeCsPins().size() > 0 && !inUse;};

	EncoderType getType(){return EncoderType::absolute;};

	void restoreFlash() override;
	void saveFlash() override;

	int32_t getPos() override;
	uint32_t getCpr() override; // Encoder counts per rotation

	int32_t getPosAbs() override;

	void setPos(int32_t pos);

	void initSPI();
	void updateAngleStatus();
	void updateAngleStatusCb();

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void setCsPin(uint8_t cspin);
	//virtual const ClassType getClassType() override {return ClassType::Encoder;};
private:
	uint8_t readSpi(uint8_t addr);
	void writeSpi(uint8_t addr,uint8_t data);
	void spiTxRxCompleted(SPIPort* port);

	volatile bool nomag = false; // Magnet lost in last report
	volatile bool overspeed = false; // Overspeed flag set in last report
	volatile int32_t lastAngleInt = 0;
	volatile int32_t curAngleInt = 0;
	volatile int32_t curPos = 0;
	volatile int32_t rotations = 0;
	int32_t offset = 0;
	uint8_t cspin = 0;
	volatile bool updateInProgress = false;

	uint8_t txbuf[4] = {0x03 | MAGNTEK_READ,0,0,0};
	uint8_t rxbuf[4] = {0,0,0,0};

};

#endif /* USEREXTENSIONS_SRC_MTENCODERSPI_H_ */
#endif
