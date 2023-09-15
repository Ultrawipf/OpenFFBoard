/*
 * SystemCommands.h
 *
 *  Created on: 30.11.2021
 *      Author: Yannick
 */

#ifndef SRC_SYSTEMCOMMANDS_H_
#define SRC_SYSTEMCOMMANDS_H_

#include "CommandHandler.h"

enum class FFBoardMain_commands : uint32_t{
	help=0,save=1,reboot=2,dfu=3,swver=4,hwtype=5,lsmain,main,lsactive,format,errors,errorsclr,flashdump,flashraw,vint,vext,mallinfo,heapfree,taskstats,debug,devid,uid,temp
};

class SystemCommands : public CommandHandler {
public:
	SystemCommands();
	virtual ~SystemCommands();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	void registerCommands();
	CommandStatus internalCommand(const ParsedCommand& cmd,std::vector<CommandReply>& replies);


	const ClassType getClassType() {return ClassType::Internal;};

	static void replyFlashDump(std::vector<CommandReply>& replies);
	static void replyErrors(std::vector<CommandReply>& replies);

	static bool debugMode; // Global flag that controls the debug mode

	static bool errorPrintingEnabled;
	static SystemCommands* systemCommandsInstance;

};

#endif /* SRC_SYSTEMCOMMANDS_H_ */
