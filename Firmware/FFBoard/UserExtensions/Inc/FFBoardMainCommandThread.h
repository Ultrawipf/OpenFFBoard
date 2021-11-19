/*
 * FFBoardMainCommandThread.h
 *
 *  Created on: Feb 13, 2021
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_FFBOARDMAINCOMMANDTHREAD_H_
#define USEREXTENSIONS_SRC_FFBOARDMAINCOMMANDTHREAD_H_


#include <CmdParser.h>
#include "cppmain.h"
#include "main.h"
#include <string>
#include "cdc_device.h"
#include "ChoosableClass.h"
#include "CommandHandler.h"
#include <vector>
#include "ErrorHandler.h"
#include "thread.hpp"
#include "FFBoardMain.h"
#include "semaphore.hpp"

#include "CommandInterface.h"

class FFBoardMain;
class CommandInterface;

class FFBoardMainCommandThread : public cpp_freertos::Thread
{
public:
	FFBoardMainCommandThread(FFBoardMain* mainclass);
	virtual ~FFBoardMainCommandThread();

	bool addBuf(char* Buf, uint32_t *Len,bool clearReply);

	void Run();

	static void wakeUp();

	FFBoardMain* main;

	Error cmdNotFoundError = Error(ErrorCode::cmdNotFound,ErrorType::temporary,"Invalid command");

	Error cmdExecError = Error(ErrorCode::cmdExecutionError,ErrorType::temporary,"Error while executing command");

	std::string cmd_reply;
	bool clearReply = true;

protected:
	virtual void updateSys();
	static void printFlashDump(std::string *reply);
	static void printErrors(std::string *reply);

	virtual void executeCommands(std::vector<ParsedCommand> commands,CommandInterface* commandInterface);
	virtual ParseStatus executeSysCommand(ParsedCommand* cmd,std::string* reply,CommandInterface* commandInterface);

	static cpp_freertos::BinarySemaphore threadSem; // Blocks this thread. more efficient than suspending/waking
	//static cpp_freertos::MutexStandard commandMutex;
};

#endif /* USEREXTENSIONS_SRC_FFBOARDMAINCOMMANDTHREAD_H_ */
