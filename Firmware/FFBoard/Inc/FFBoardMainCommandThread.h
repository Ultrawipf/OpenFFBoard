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
//class CommandInterface;


class FFBoardMainCommandThread : public cpp_freertos::Thread
{
public:
	FFBoardMainCommandThread(FFBoardMain* mainclass);
	virtual ~FFBoardMainCommandThread();

//	const ClassIdentifier getInfo();
//	static ClassIdentifier info;
	const ClassType getClassType(){return ClassType::Internal;};

	bool addBuf(char* Buf, uint32_t *Len,bool clearReply);

	void Run();

	static void wakeUp();

	//FFBoardMain* main;

	std::string cmd_reply;
	std::vector<CommandResult> results; // Stores the results until the next batch to pass back to the interface
	std::vector<ParsedCommand> commands;
	bool clearReply = true;

	static Error cmdNotFoundError;
	static Error cmdExecError;

protected:
	virtual void executeCommands(std::vector<ParsedCommand>& commands,CommandInterface* commandInterface);


	//static cpp_freertos::BinarySemaphore threadSem; // Blocks this thread. more efficient than suspending/waking
};

#endif /* USEREXTENSIONS_SRC_FFBOARDMAINCOMMANDTHREAD_H_ */
