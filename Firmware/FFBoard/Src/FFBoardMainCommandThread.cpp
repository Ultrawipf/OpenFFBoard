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
#include "cpp_target_config.h"



Error FFBoardMainCommandThread::cmdNotFoundError = Error(ErrorCode::cmdNotFound,ErrorType::temporary,"Invalid command");
Error FFBoardMainCommandThread::cmdExecError = Error(ErrorCode::cmdExecutionError,ErrorType::temporary,"Error while executing command");


FFBoardMainCommandThread* commandThread;

// Note: allocate enough memory for the command thread to store replies
FFBoardMainCommandThread::FFBoardMainCommandThread(FFBoardMain* mainclass) : Thread("CMD_MAIN",700, 32) {
	//main = mainclass;
	commandThread = this;
	this->Start();

}

FFBoardMainCommandThread::~FFBoardMainCommandThread() {

}



void FFBoardMainCommandThread::Run(){
	while(true){
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
		WaitForNotification();
		//FFBoardMainCommandThread::threadSem.Take(); // Stop thread again. will be resumed when parser ready

		//Delay(1); // Give the scheduler time
	}
}

/**
 * Signals the command thread that new commands are ready to be executed
 */
void FFBoardMainCommandThread::wakeUp(){

	if(inIsr()){
		commandThread->NotifyFromISR();
//		BaseType_t pxHigherPriorityTaskWoken;
//		FFBoardMainCommandThread::threadSem.GiveFromISR(&pxHigherPriorityTaskWoken);
//		portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
	}else{
		commandThread->Notify();
		//FFBoardMainCommandThread::threadSem.Give();
	}

}






/**
 * Executes parsed commands and calls other command handlers.
 * Not global so it can be overridden by main classes to change behaviour or suppress outputs.
 */
void FFBoardMainCommandThread::executeCommands(std::vector<ParsedCommand>& commands,CommandInterface* commandInterface){
	for(ParsedCommand& cmd : commands){
		this->results.clear();
		cmd.originalInterface = commandInterface;

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
			this->results.emplace_back(); // Create new result element
			CommandResult& resultObj = this->results.back();
			status = handler->internalCommand(cmd,resultObj.reply);

			// internal commands did not return anything call regular custom commands
			if(status == CommandStatus::NOT_FOUND && handler->hasCommands()){
				status = handler->command(cmd,resultObj.reply);
			}
			// If status is not no reply append a reply object. If command was not found the reply vector should be empty but the not found flag set
			if(status != CommandStatus::NO_REPLY){
				resultObj.handlerId = handler->getCommandHandlerID();
				resultObj.originalCommand = cmd;
				resultObj.type = status;
				resultObj.commandHandler = handler;
			}else{
				this->results.pop_back(); // Remove result again
			}
		}
		// Results are buffered. send out
		if(!this->results.empty()){
			for(CommandInterface* itf : CommandInterface::cmdInterfaces){
				// Block until replies are sent
				uint32_t remainingTime = 100;
				while(!itf->readyToSend() && --remainingTime){
					Delay(1);
				}
				if(remainingTime){
					itf->sendReplies(results, commandInterface);
				}
			}
		}
	}
	for(CommandInterface* itf : CommandInterface::cmdInterfaces){
		itf->batchDone(); // Signal that all commands are done
	}

}
