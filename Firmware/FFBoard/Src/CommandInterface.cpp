/*
 * CommandInterface.cpp
 *
 *  Created on: 17.11.2021
 *      Author: Yannick
 */

#include "CommandInterface.h"
#include "CDCcomm.h"
#include "global_callbacks.h"
#include "CommandHandler.h"
#include <stdlib.h>
#include "critical.hpp"

std::vector<CommandInterface*> CommandInterface::cmdInterfaces;


CommandInterface::CommandInterface() {
	addCallbackHandler(CommandInterface::cmdInterfaces, this);
}

CommandInterface::~CommandInterface() {
	removeCallbackHandler(CommandInterface::cmdInterfaces, this);
}


/**
 * Broadcasts an unrequested reply to all command interfaces
 */
void CommandInterface::broadcastCommandReplyAsync(const std::vector<CommandReply>& reply,CommandHandler* handler, uint32_t cmdId,CMDtype type){

	const std::vector<CommandResult> results = {makeCommandReply(reply, handler, cmdId, type,nullptr)};

	for(CommandInterface* itf : CommandInterface::cmdInterfaces){
		itf->sendReplies(results, nullptr);
	}
}

/**
 * Sends an unrequested reply to this specific interface
 */
void CommandInterface::sendReplyAsync(std::vector<CommandReply>& reply,CommandHandler* handler, uint32_t cmdId,CMDtype type){
	std::vector<CommandResult> results = {makeCommandReply(reply, handler, cmdId, type,this)};
	this->sendReplies(results, this);
}

CommandResult CommandInterface::makeCommandReply(const std::vector<CommandReply>& reply,CommandHandler* handler, uint32_t cmdId,CMDtype type,CommandInterface* originalInterface){
	ParsedCommand fakeCmd;
	fakeCmd.target = handler;
	fakeCmd.cmdId = cmdId;
	fakeCmd.instance = handler->getCommandHandlerInfo()->instance;
	fakeCmd.type = type;
	fakeCmd.originalInterface = originalInterface;


	CommandResult fakeResult;
	fakeResult.type = CommandStatus::BROADCAST;
	fakeResult.handlerId = fakeCmd.target->getCommandHandlerID();
	fakeResult.originalCommand = fakeCmd;
	fakeResult.reply = reply;
	fakeResult.commandHandler = fakeCmd.target;
	return fakeResult;
}

/**
 * Command thread requests new commands
 * Return true if any are ready, false otherwise
 * If true it will request the vector of parsed commands
 */
bool CommandInterface::hasNewCommands(){
	return parserReady;
}

/**
 * Returns true if it can send replies.
 * False if the command thread should wait until previous replies are processed
 */
bool CommandInterface::readyToSend(){
	return true;
}

/**
 * Command thread signals that all commands in this batch are done
 */
void CommandInterface::batchDone(){

}


/*
 *
 * **********************************************************
 *
 */

bool StringCommandInterface::getNewCommands(std::vector<ParsedCommand>& commands){
	parserReady = false;
	return parser.parse(commands);
}

/*
 * Adds a buffer to the parser
 * if it returns true resume the thread
 */
bool StringCommandInterface::addBuf(char* Buf, uint32_t *Len){
	bool res = this->parser.add(Buf, Len);
	if(res){
		parserReady = true; // Signals that we should execute commands in the thread
		FFBoardMainCommandThread::wakeUp();
	}
	return res;
}

uint32_t StringCommandInterface::bufferCapacity(){
	return parser.bufferCapacity();
}

void StringCommandInterface::formatReply(std::string& reply,const std::vector<CommandResult>& results,const bool formatWriteAsRead){
	//uint16_t lastId = 0xFFFF;
	for(const CommandResult& result : results){ // All commands processed this batch
		if(formatWriteAsRead && !(result.originalCommand.type == CMDtype::set || result.originalCommand.type == CMDtype::setat)){
			return; // Ignore commands that should be formatted as read if they are not set commands
		}

		reply += '['; // Start marker
		reply += StringCommandInterface::formatOriginalCommandFromResult(result.originalCommand,result.commandHandler, formatWriteAsRead);
		reply += '|'; // Separator

		if(formatWriteAsRead){
			std::string tstr;
			StringCommandInterface::generateReplyFromCmd(tstr,result.originalCommand);
			reply += tstr;
		}else{
			if(result.type == CommandStatus::NOT_FOUND){
				reply += "NOT_FOUND";
				//ErrorHandler::addError(CommandInterface::cmdNotFoundError);
			}else if(result.type == CommandStatus::OK || result.type == CommandStatus::BROADCAST){
				if(result.reply.empty()){
					reply += "OK";
				}else{
					uint16_t repliesRemaining = result.reply.size();
					for(const CommandReply& cmdReply : result.reply){ // For all entries of this command. Normally just one
						std::string tstr;
						//TODO Long replies fail HERE while copying reply strings
						StringCommandInterface::generateReplyValueString(tstr,cmdReply);
						reply += tstr;
						if(--repliesRemaining > 0){
							reply += "\n"; // Separate replies with newlines
						}
					}
				}
			}else if(result.type == CommandStatus::ERR){
				reply += "ERR";
			}else{
				reply += "None";
			}
		}

		reply += "]\n"; // end marker
	}
}

/**
 * Formats a command string from a reply result
 */
std::string StringCommandInterface::formatOriginalCommandFromResult(const ParsedCommand& originalCommand,CommandHandler* commandHandler,const bool formatWriteAsRead){

//	ClassIdentifier info = commandHandler->getInfo();
	std::string cmdstring = commandHandler->getCommandHandlerInfo()->clsname;
	if(originalCommand.instance != 0xFF){
		cmdstring += '.';
		cmdstring += std::to_string(originalCommand.instance);
	}
	cmdstring += '.';


	CmdHandlerCommanddef* cmdDef = commandHandler->getCommandFromId(originalCommand.cmdId); // CMD name
	if(!cmdDef){
		return ""; // can not find cmd. should never happen
	}
	cmdstring += std::string(cmdDef->cmd);
	if(originalCommand.type == CMDtype::get || (formatWriteAsRead && originalCommand.type == CMDtype::set)){
		cmdstring += '?';

	}else if(originalCommand.type == CMDtype::getat || (formatWriteAsRead && originalCommand.type == CMDtype::setat)){ // cls.inst.cmd?
		cmdstring += '?';
		cmdstring += std::to_string(originalCommand.adr);

	}else if(originalCommand.type == CMDtype::set){ // cls.inst.cmd?
		cmdstring += '=';
		cmdstring += std::to_string(originalCommand.val);

	}else if(originalCommand.type == CMDtype::setat){ // cls.inst.cmd?x=y
		cmdstring += '=';
		cmdstring += std::to_string(originalCommand.val);
		cmdstring += '?';
		cmdstring += std::to_string(originalCommand.adr);

	}else if(originalCommand.type == CMDtype::info){
		cmdstring += '!';
	}

	return cmdstring;

}

/**
 * Creates the value part of the reply string
 */
void StringCommandInterface::generateReplyValueString(std::string& replyPart,const CommandReply& reply){
	//std::string replyPart;
	if(reply.type == CommandReplyType::STRING || reply.type == CommandReplyType::STRING_OR_INT || reply.type == CommandReplyType::STRING_OR_DOUBLEINT){
		replyPart.assign(reply.reply);
	}else if(reply.type == CommandReplyType::INT){
		replyPart = std::to_string(reply.val);
	}else if(reply.type == CommandReplyType::DOUBLEINTS){
		replyPart = std::to_string(reply.val);
		replyPart += ':';
		replyPart += std::to_string(reply.adr);
	}else if(reply.type == CommandReplyType::ACK){
		replyPart = "OK";
	}
}

void StringCommandInterface::generateReplyFromCmd(std::string& replyPart,const ParsedCommand& originalCommand){
	if(originalCommand.type == CMDtype::set){
		replyPart = std::to_string(originalCommand.val);
	}else if(originalCommand.type == CMDtype::setat){
		replyPart = std::to_string(originalCommand.val);
		replyPart += ':';
		replyPart += std::to_string(originalCommand.adr);
	}
}


/*
 *
 * **********************************************************
 *
 */


CDC_CommandInterface::CDC_CommandInterface() : StringCommandInterface(1024), Thread("CDCCMD", 512, 37) {
	parser.setClearBufferTimeout(parserTimeout);
	this->Start();
}

CDC_CommandInterface::~CDC_CommandInterface() {

}


void CDC_CommandInterface::Run(){
	while(true){
		WaitForNotification();

		this->sendBuffer.clear();
		if(this->sendBuffer.capacity() > 200){
			this->sendBuffer.reserve(64);
		}
		StringCommandInterface::formatReply(sendBuffer,resultsBuffer,nextFormat);
		resultsBuffer.clear();
		bufferLength = 0;
		if(!sendBuffer.empty())
			CDCcomm::cdcSend(&sendBuffer, 0);
	}
}

void CDC_CommandInterface::sendReplies(const std::vector<CommandResult>& results,CommandInterface* originalInterface){
	if(!tud_ready()){
		return; //if not ready we will discard it anyways
	}

	if(HAL_GetTick() - lastSendTime > parserTimeout){
		bufferLength = 0;
		resultsBuffer.clear(); // Empty buffer because we were not able to reply in time to prevent the full buffer from blocking future commands
		//CDCcomm::clearRemainingBuffer(0);
	}

	if( (!enableBroadcastFromOtherInterfaces && originalInterface != this) ){
		return;
	}
	for(const CommandResult& r : results){
		bufferLength += r.size();
	}

	resultsBuffer.insert(resultsBuffer.end(), results.begin(), results.end());
	//resultsBuffer.shrink_to_fit();
	nextFormat = originalInterface != this && originalInterface != nullptr;
	lastSendTime = HAL_GetTick();
	if(bufferLength >= maxSendBuffer){
		Notify(); // Resume
	}

}

void CDC_CommandInterface::batchDone(){
	if(resultsBuffer.empty())
		return;
	resultsBuffer.shrink_to_fit();
	Notify();
}

/**
 * Ready to send if there is no data in the backup buffer of the cdc port
 */
bool CDC_CommandInterface::readyToSend(){
	if(!tud_ready() || !tud_cdc_connected()){ // TODO check if cdc connected. if not connected be ready but ignore data.
		return true;
	}
	if(HAL_GetTick() - lastSendTime > parserTimeout && CDCcomm::remainingData(0) == 0){
		return true;
	}
	return CDCcomm::remainingData(0) == 0 && bufferLength < maxSendBuffer; //&& resultsBuffer.empty()
}



/*************************
 *
 * Uart command interface
 * Takes bytes from a uart port
 *
 */

extern UARTPort external_uart; // defined in cpp_target_config.cpp
UART_CommandInterface::UART_CommandInterface(uint32_t baud) : UARTDevice(external_uart),Thread("UARTCMD", 150, 36),StringCommandInterface(512), baud(baud){ //
	uartconfig = uartport->getConfig();
	if(baud != 0){
		uartconfig.BaudRate = this->baud;
		uartport->reconfigurePort(uartconfig);
	}
	parser.setClearBufferTimeout(parserTimeout);
	uartport->registerInterrupt(); // enable port
	this->Start();
}

UART_CommandInterface::~UART_CommandInterface() {

}

/**
 * Ready to send if there is no data in the backup buffer of the cdc port
 */
bool UART_CommandInterface::readyToSend(){

	return !uartport->isTaken() && bufferLength < maxSendBuffer;
}

void UART_CommandInterface::Run(){
	while(true){

		WaitForNotification();
		while(uartport->isTaken()){
			Delay(1);
		}
		this->sendBuffer.clear();
		if(this->sendBuffer.capacity() > 100){
			this->sendBuffer.reserve(64);
		}
		StringCommandInterface::formatReply(sendBuffer,resultsBuffer,nextFormat);
		resultsBuffer.clear();
		bufferLength = 0;
		if(!sendBuffer.empty())
			uartport->transmit_IT(sendBuffer.c_str(), sendBuffer.size());
	}
}


void UART_CommandInterface::sendReplies(const std::vector<CommandResult>& results,CommandInterface* originalInterface){
	if( (!enableBroadcastFromOtherInterfaces && originalInterface != this) ){
		return;
	}

	for(const CommandResult& r : results){
		bufferLength += r.size();
	}

	resultsBuffer.insert(resultsBuffer.end(), results.begin(), results.end());
	//resultsBuffer.shrink_to_fit();
	nextFormat = originalInterface != this && originalInterface != nullptr;
	if(bufferLength >= maxSendBuffer){
		Notify(); // Resume
	}
}

/**
 * Receives one byte and adds it to the parser
 */
void UART_CommandInterface::uartRcv(char& buf){
	uint32_t len = 1;
	//BaseType_t savedInterruptStatus =  cpp_freertos::CriticalSection::EnterFromISR();
	StringCommandInterface::addBuf(&buf, &len);
	uartport->registerInterrupt();

	//cpp_freertos::CriticalSection::ExitFromISR(savedInterruptStatus);
}

void UART_CommandInterface::batchDone(){
	if(resultsBuffer.empty())
		return;
	resultsBuffer.shrink_to_fit();
	Notify();
}

