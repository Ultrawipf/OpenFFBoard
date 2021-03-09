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

class FFBoardMain;
class FFBoardMainCommandThread : public cpp_freertos::Thread
{
public:
	FFBoardMainCommandThread(FFBoardMain* mainclass);
	virtual ~FFBoardMainCommandThread();

	bool addBuf(char* Buf, uint32_t *Len,bool clearReply);

	void Run();
	FFBoardMain* main;

	Error_t cmdNotFoundError = {
		code : ErrorCode::cmdNotFound,
		type : ErrorType::temporary,
		info : "Invalid command"
	};

	Error_t cmdExecError = {
		code : ErrorCode::cmdExecutionError,
		type : ErrorType::temporary,
		info : "Error while executing command"
	};

	CmdParser parser = CmdParser();
	bool parserReady = false;
	std::string cmd_reply;
	bool clearReply = true;

	static std::string getHelpstring(){return "\nSystem Commands: errors,reboot,help,dfu,swver (Version),hwtype,lsmain (List configs),id,main (Set main config),lsactive (print command handlers),vint,vext,format (Erase flash),mallinfo,heapfree (Mem usage),flashdump,flashraw\n";}
protected:
	virtual void updateSys();
	static void printFlashDump(std::string *reply);
	static void printErrors(std::string *reply);

	virtual void executeCommands(std::vector<ParsedCommand> commands);
	virtual ParseStatus executeSysCommand(ParsedCommand* cmd,std::string* reply);

};

#endif /* USEREXTENSIONS_SRC_FFBOARDMAINCOMMANDTHREAD_H_ */
