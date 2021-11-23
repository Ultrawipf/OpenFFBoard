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
#include <malloc.h>
#include "FFBoardMain.h"
#include "critical.hpp"

extern ClassChooser<FFBoardMain> mainchooser;

cpp_freertos::BinarySemaphore FFBoardMainCommandThread::threadSem = cpp_freertos::BinarySemaphore();


FFBoardMainCommandThread::FFBoardMainCommandThread(FFBoardMain* mainclass) : Thread("cmdparser",512, 39) {
	main = mainclass;
	this->Start();
}

FFBoardMainCommandThread::~FFBoardMainCommandThread() {

}


void FFBoardMainCommandThread::updateSys(){
//	if(this->parserReady){
//		this->parserReady = false;
//		executeCommands(this->parser.parse()); // Don't call this in interrupts!
//	}
	// Ask command interfaces if new commands are available if woken up
	for(CommandInterface* itf : CommandInterface::cmdInterfaces){
		if(itf->hasNewCommands()){
			this->executeCommands(itf->getNewCommands(), itf);
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

void FFBoardMainCommandThread::wakeUp(){

	if(inIsr()){
		BaseType_t pxHigherPriorityTaskWoken;
		FFBoardMainCommandThread::threadSem.GiveFromISR(&pxHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
	}else{
		FFBoardMainCommandThread::threadSem.Give();
	}

}


/*
 * Prints a formatted flash dump to the reply string
 */
void FFBoardMainCommandThread::printFlashDump(std::string *reply){
	std::vector<std::tuple<uint16_t,uint16_t>> result;

	Flash_Dump(&result);
	uint16_t adr;
	uint16_t val;

	for(auto entry : result){
		std::tie(adr,val) = entry;
		*reply += std::to_string(adr) + ":" + std::to_string(val) + "\n";
	}

}

/*
 * Prints a formatted list of error conditions
 */
void FFBoardMainCommandThread::printErrors(std::string *reply){
	std::vector<Error>* errors = ErrorHandler::getErrors();
	if(errors->size() == 0){
		*reply += "None";
		return;
	}

	for(Error error : *errors){
		*reply += error.toString() + "\n";
	}

	ErrorHandler::clearTemp();
}

ParseStatus FFBoardMainCommandThread::executeSysCommand(ParsedCommand* cmd,std::string* reply,CommandInterface* commandInterface){
	ParseStatus flag = ParseStatus::OK;

	if(cmd->cmd == "help"){
		std::string help, last_help = "";
		*reply += commandInterface->getHelpstring();
		for(CommandHandler* handler : CommandHandler::cmdHandlers){
			help = handler->getHelpstring();
			if (help != last_help) { // avoid duplicate help commands from multiple instances
				*reply += help;
				last_help = help;
			}
		}

	}else if(cmd->cmd == "save"){
		for(PersistentStorage* handler : PersistentStorage::flashHandlers){
			handler->saveFlash();
		}

	}else if(cmd->cmd == "reboot"){
		NVIC_SystemReset();
	}else if(cmd->cmd == "dfu"){ // Reboot into DFU bootloader mode
		RebootDFU();
	}else if(cmd->cmd == "vint"){
		if(cmd->type==CMDtype::get){
			*reply+=std::to_string(getIntV());
		}

	}else if(cmd->cmd == "vext"){
		if(cmd->type==CMDtype::get){
			*reply+=std::to_string(getExtV());
		}

	}else if(cmd->cmd == "swver"){
		*reply += (SW_VERSION);

	}else if(cmd->cmd == "minVerGui"){
		*reply += (MIN_SW_CONFIGURATOR);

	}else if(cmd->cmd == "hwtype"){
		*reply += (HW_TYPE);

	}else if(cmd->cmd == "flashdump"){
		printFlashDump(reply);

	}else if(cmd->cmd == "errors"){
		printErrors(reply);

	}else if(cmd->cmd == "errorsclr"){
		ErrorHandler::clearAll();

	}else if(cmd->cmd == "flashraw"){ // Set and get flash eeprom emulation values
		if(cmd->type == CMDtype::setat){
			Flash_Write(cmd->adr, cmd->val);

		}else if(cmd->type == CMDtype::getat){
			uint16_t val;
			if(Flash_Read(cmd->adr,&val)){
				*reply+=std::to_string(val);
			}
			else{
				flag = ParseStatus::ERR;
			}
		}

	}else if(cmd->type!=CMDtype::set &&cmd->cmd == "lsmain"){
		*reply += mainchooser.printAvailableClasses();

	}else if(cmd->cmd == "id"){ // Report id of main class
		*reply+=std::to_string(main->getInfo().id);

	}else if(cmd->cmd == "mallinfo"){
		struct mallinfo info = mallinfo();
		*reply+="Usage: ";
		*reply+=std::to_string(info.uordblks);
		*reply+=" Size: ";
		*reply+=std::to_string(info.arena);

	}else if(cmd->cmd == "heapfree"){ // Free rtos memory
		*reply+=std::to_string(xPortGetFreeHeapSize());

	}else if(cmd->cmd == "lsactive"){ // Prints all active command handlers that have a name
		for(CommandHandler* handler : CommandHandler::cmdHandlers){
			if(handler->hasCommands()){
				ClassIdentifier i = handler->getInfo();
				//if(!i.hidden)
				*reply += std::string(i.name) + ":" + std::to_string(i.id) + ":" + i.unique + ":" + std::to_string(handler->getCommandHandlerID()) + "\n";
			}
		}

	}else if(cmd->cmd == "main"){
		if(cmd->type == CMDtype::get || cmd->type == CMDtype::none){
			uint16_t buf=main->getInfo().id;
			Flash_Read(ADR_CURRENT_CONFIG, &buf);
			*reply+=std::to_string(buf);

		}else if(cmd->type == CMDtype::set){
			if(mainchooser.isValidClassId(cmd->val)){
				Flash_Write(ADR_CURRENT_CONFIG, (uint16_t)cmd->val);
				if(cmd->val != main->getInfo().id){
					NVIC_SystemReset(); // Reboot
				}
			}else if(cmd->type == CMDtype::err){
				flag = ParseStatus::ERR;
			}
		}
	}else if(cmd->cmd == "format"){
		if(cmd->type == CMDtype::set && cmd->val==1){
			HAL_FLASH_Unlock();
			EE_Format();
			HAL_FLASH_Lock();
		}else{
			*reply += "format=1 will ERASE ALL stored variables. Be careful!";
		}
	}else{
		// No command found
		flag = ParseStatus::NOT_FOUND;
	}

	return flag;
}

/**
 * Executes parsed commands and calls other command handlers.
 * Not global so it can be overridden by main classes to change behaviour or suppress outputs.
 */
void FFBoardMainCommandThread::executeCommands(std::vector<ParsedCommand> commands,CommandInterface* commandInterface){
	if(clearReply)
		this->results.clear();
		//this->cmd_reply.clear();
	for(ParsedCommand cmd : commands){
		ParseStatus status = ParseStatus::NOT_FOUND;
		if(cmd.cmd.empty())
			continue; // Empty command

		//this->cmd_reply+= ">" + cmd.rawcmd + ":"; // Start marker. Echo command

		std::string reply; // Reply of current command
		status = executeSysCommand(&cmd,&reply,commandInterface);
		if(status == ParseStatus::NOT_FOUND || status == ParseStatus::OK_CONTINUE){ // Not a system command
			// Call all command handlers

			// Prevent other threads from running while we call all command handlers
			// TODO monitor if this causes problems or is really needed.
			//cpp_freertos::CriticalSection::SuspendScheduler();
			for(CommandHandler* handler : CommandHandler::cmdHandlers){
				if(handler->hasCommands()){
					ParseStatus newstatus = handler->command(&cmd,&reply);
					// If last class did not have commands but a previous one asked to continue keep continue status
					if(!(status == ParseStatus::OK_CONTINUE && newstatus == ParseStatus::NOT_FOUND)){
						status = newstatus;
					}
					if(status == ParseStatus::ERR || status == ParseStatus::OK || status == ParseStatus::NO_REPLY)
						break; // Stop after this class if finished flag is returned
				}
			}
			// Resume scheduler
			//cpp_freertos::CriticalSection::ResumeScheduler();
		}
		if(status == ParseStatus::NO_REPLY){
			cmd_reply.clear();
			continue; // don't send reply. Just continue
		}

		// Make the reply object. TODO let class do this
		CommandReply replyObj;
		replyObj.reply = reply;
		replyObj.result = status;

		// The result to be returned to the interfaces
		CommandResult resultObj = {
				.reply = replyObj,
				.originalCommand = cmd
		};
		this->results.push_back(resultObj);
	}
	if(!this->results.empty()){
		for(CommandInterface* itf : CommandInterface::cmdInterfaces){
			itf->sendReplies(results, commandInterface);
		}
	}

}
