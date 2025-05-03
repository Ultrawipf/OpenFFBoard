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
		caniddigital,canidanalog,btntypes,lsbtn,addbtn,aintypes,lsain,addain,rate,dvals,avals
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

	void setReportRate(uint8_t rateidx);
	std::string report_rates_names();


protected:
	uint32_t report_rate_cnt = 0;
	uint32_t report_rate = 1;

	std::vector<int32_t> analogBuffer;
	uint64_t digitalBuffer;

	CANPort& can;

	uint32_t buttons_id = 100;
	uint32_t analog_id = 110;

	uint8_t rate_idx = 0;
	const std::array<uint8_t,7> report_rates = {1,2,4,8,10,16,32}; // Maps speed preset to report rates


};
#endif
#endif /* USEREXTENSIONS_INC_CANINPUTMAIN_H_ */
