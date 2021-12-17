/*
 * FFBoardMainCommandThread.cpp
 *
 *  Created on: Feb 13, 2021
 *      Author: Yannick
 */

#include "FFBoardMainCommandThread.h"
#include <thread.hpp>
#include "constants.h"
#include "mainclass_chooser.h"
#include "eeprom_addresses.h"
#include "flash_helpers.h"
#include "eeprom.h"
#include "voltagesense.h"
#include "global_callbacks.h"
#include "PersistentStorage.h"
#include "ErrorHandler.h"
#include "ClassChooser.h"
#include "FFBoardMain.h"
#include "critical.hpp"


Error FFBoardMainCommandThread::cmdNotFoundError = Error(ErrorCode::cmdNotFound,ErrorType::temporary,"Invalid command");
Error FFBoardMainCommandThread::cmdExecError = Error(ErrorCode::cmdExecutionError,ErrorType::temporary,"Error while executing command");


cpp_freertos::BinarySemaphore FFBoardMainCommandThread::threadSem = cpp_freertos::BinarySemaphore();


// Note: allocate enough memory for the command thread to store replies
FFBoardMainCommandThread::FFBoardMainCommandThread(FFBoardMain* mainclass) : Thread("cmdparser",1024, 35) {
	//main = mainclass;
	this->Start();
}

FFBoardMainCommandThread::~FFBoardMainCommandThread() {

}


void FFBoardMainCommandThread::updateSys(){

	// Ask command interfaces if new commands are available if woken up
	for(CommandInterface* itf : CommandInterface::cmdInterfaces){
		if(itf->hasNewCommands()){
			itf->getNewCommands(commands);
			this->executeCommands(commands, itf);
			commands.clear();
			if(commands.capacity() > 20)
				commands.shrink_to_fit();
		}
	}

	FFBoardMainCommandThread::threadSem.Take(); // Stop thread again. will be resumed when parser ready
}

void FFBoardMainCommandThread::Run(){
	while(true){
		updateSys();
		Delay(1); // Give the scheduler time
	}
}

/**
 * Signals the command thread that new commands are ready to be executed
 */
void FFBoardMainCommandThread::wakeUp(){

	if(inIsr()){
		BaseType_t pxHigherPriorityTaskWoken;
		FFBoardMainCommandThread::threadSem.GiveFromISR(&pxHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
	}else{
		FFBoardMainCommandThread::threadSem.Give();
	}

}






/**
 * Executes parsed commands and calls other command handlers.
 * Not global so it can be overridden by main classes to change behaviour or suppress outputs.
 */
void FFBoardMainCommandThread::executeCommands(std::vector<ParsedCommand> commands,CommandInterface* commandInterface){

	//cpp_freertos::CriticalSection::SuspendScheduler();
	for(ParsedCommand& cmd : commands){
		this->results.clear();

		CommandResult resultObj;
		CommandStatus status = CommandStatus::NOT_FOUND;
		CommandHandler* handler = cmd.target;

		CmdHandlerCommanddef* cmdDef = handler->getCommandFromId(cmd.cmdId);
		// check flags
		bool validFlags = cmdDef != nullptr;
		if(cmdDef->flags & ( CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING | CMDFLAG_GETADR | CMDFLAG_SETADR) ){
			// A type flag is preset. check it
			validFlags = static_cast<uint32_t>(cmd.type) & cmdDef->flags; // type uses the same flag values
		}
		if(CommandHandler::isInHandlerList(handler)  && validFlags){ // check if pointer is still present in handler list
			// Call internal commands first
			status = handler->internalCommand(cmd,resultObj.reply,commandInterface);

			// internal commands did not return anything call regular custom commands
			if(status == CommandStatus::NOT_FOUND){
				status = handler->command(cmd,resultObj.reply);
			}

		}
		// If status is not no reply append a reply object. If command was not found the reply vector should be empty but the not found flag set
		if(status != CommandStatus::NO_REPLY){
			resultObj.handlerId = handler->getCommandHandlerID();
			resultObj.originalCommand = cmd;
			resultObj.type = status;
			resultObj.commandHandler = handler;
			//this->results.push_back(resultObj);
			// Not found if result is empty
			this->results.push_back(resultObj);
		}
		if(!this->results.empty()){
			for(CommandInterface* itf : CommandInterface::cmdInterfaces){
				itf->sendReplies(results, commandInterface);
			}
		}
	}
	//cpp_freertos::CriticalSection::ResumeScheduler();
}
