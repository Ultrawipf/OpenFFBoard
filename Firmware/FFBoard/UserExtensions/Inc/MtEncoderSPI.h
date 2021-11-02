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

	ParseStatus command(ParsedCommand* cmd,std::string* reply);
	void setCsPin(uint8_t cspin);

private:
	uint8_t readSpi(uint8_t addr);
	void writeSpi(uint8_t addr,uint8_t data);

	bool nomag = false; // Magnet lost in last report
	bool overspeed = false; // Overspeed flag set in last report
	int32_t lastAngleInt = 0;
	int32_t curAngleInt = 0;
	int32_t curPos = 0;
	int32_t rotations = 0;
	int32_t offset = 0;
	uint8_t cspin = 0;
};

#endif /* USEREXTENSIONS_SRC_MTENCODERSPI_H_ */
#endif
