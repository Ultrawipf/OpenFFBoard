/*
 * CanButtons.h
 *
 *  Created on: 06.04.2022
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_CANBUTTONS_H_
#define USEREXTENSIONS_SRC_CANBUTTONS_H_

#include "constants.h"
#ifdef CANBUTTONS

#include "ButtonSource.h"
#include "PersistentStorage.h"
#include "CAN.h"
#include "CanHandler.h"
#include "CommandHandler.h"
#include "cpp_target_config.h"

//class CanHandler;
class CanButtons : public ButtonSource, public CanHandler, public CommandHandler {
public:
	enum class CanButtons_commands : uint32_t {
		btnnum,invert,canid
	};

	CanButtons();
	virtual ~CanButtons();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	void restoreFlash();
	void saveFlash();

	uint8_t readButtons(uint64_t* buf);
	void setupCanPort();
	void setBtnNum(uint8_t num);

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);

	void canRxPendCallback(CANPort* port,CAN_rx_msg& msg) override;

private:
	bool invert = false;
	uint32_t canId = 100;
	int32_t filterId = -1;
	CANPort* port = &canport;
	uint64_t mask = 0xff;

	volatile uint64_t currentButtons = 0;
};


#endif
#endif /* USEREXTENSIONS_SRC_CANBUTTONS_H_ */
