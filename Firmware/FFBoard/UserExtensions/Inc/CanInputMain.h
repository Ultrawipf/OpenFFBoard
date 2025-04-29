/*
 * CanInputMain.h
 *
 *  Created on: Apr 28, 2025
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_INC_CANINPUTMAIN_H_
#define USEREXTENSIONS_INC_CANINPUTMAIN_H_
#include "constants.h"
#ifdef CANINPUTMAIN

#include "FFBoardMain.h"
#include "PersistentStorage.h"
#include "ButtonSource.h"
#include "AnalogSource.h"
#include "thread.hpp"
#include "CAN.h"
#include "SelectableInputs.h"

class CANInputMain : public FFBoardMain, public PersistentStorage, public SelectableInputs, public cpp_freertos::Thread {
	enum class CANInput_commands : uint32_t{
		caniddigital,canidanalog,btntypes,lsbtn,addbtn,aintypes,lsain,addain
	};
public:
	CANInputMain();
	CANInputMain(CANPort& canport);
	virtual ~CANInputMain();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	static bool isCreatable() {return true;};

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands();
	virtual std::string getHelpstring(){
		return "Remote CAN Analog/Digital source";
		}

	void usbInit() override;

	void saveFlash();
	void restoreFlash();

	void Run();

	void updateControl();
	void sendReport();


protected:
	uint16_t btnsources = 0; // Disabled by default
	uint16_t ainsources = 0;

	uint32_t report_rate_cnt = 0;
	uint32_t report_rate;

	std::vector<int32_t> analogBuffer;
	uint64_t digitalBuffer;

	CANPort& can;

	uint32_t buttons_id = 100;
	uint32_t analog_id = 110;
//	ClassChooser<ButtonSource> btn_chooser;
//	ClassChooser<AnalogSource> analog_chooser;


};
#endif
#endif /* USEREXTENSIONS_INC_CANINPUTMAIN_H_ */
