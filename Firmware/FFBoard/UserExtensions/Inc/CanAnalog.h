/*
 * CanAnalog.h
 *
 *  Created on: 06.04.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_CanAnalog_H_
#define USEREXTENSIONS_SRC_CanAnalog_H_

#include "constants.h"
#ifdef CANANALOG

#include "AnalogSource.h"
#include "PersistentStorage.h"
#include "CAN.h"
#include "CanHandler.h"
#include "CommandHandler.h"
#include "cpp_target_config.h"


class CanAnalogBase : public AnalogSource, public CanHandler, public CommandHandler{
public:
	enum class CanAnalog_commands : uint32_t {
		canid,amount,maxAmount
	};

	CanAnalogBase(uint8_t maxAxes = 8);
	virtual ~CanAnalogBase();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	void restoreFlash();
	void saveFlash();

	std::string getHelpstring(){return "4 16b axes per 64b packet";}


	void setupCanPort();
	void setAxisNum(uint8_t num);

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);

	void canRxPendCallback(CANPort* port,CAN_rx_msg& msg) override;

private:
	uint32_t canId = 110;
	int32_t filterId = -1;
	CANPort* port = &canport;
	uint8_t axes = 1;
	const uint8_t maxAxes;
	uint8_t packets = 1;
};

/**
 * Helper class to pass the max amount of buttons as a template parameter for classchooser
 */
template<uint8_t AMOUNT>
class CanAnalog : public CanAnalogBase
{
public:
	CanAnalog() : CanAnalogBase(AMOUNT){}
	~CanAnalog(){}
};


#endif
#endif /* USEREXTENSIONS_SRC_CanAnalog_H_ */
