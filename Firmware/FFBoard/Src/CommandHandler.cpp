/*
 * CommandHandler.cpp
 *
 *  Created on: 03.04.2020
 *      Author: Yannick
 */

#include "CommandHandler.h"
#include "global_callbacks.h"
#include "FFBoardMain.h"
#include "cdc_device.h"
#include <set>

std::vector<CommandHandler*> CommandHandler::cmdHandlers;
std::set<uint16_t> CommandHandler::cmdHandlerIDs;
bool CommandHandler::logEnabled = true; // If logs are sent by default

CommandHandler::CommandHandler() {
	addCommandHandler();
}


CommandHandler::~CommandHandler() {
	// Remove from global list when deleted
	removeCommandHandler();
}

bool CommandHandler::hasCommands(){
	return this->commandsEnabled;
}
void CommandHandler::setCommandsEnabled(bool enable){
	this->commandsEnabled = enable;
}

bool CommandHandler::logsEnabled(){
	return logEnabled;
}

/**
 * Enables or disables logs sent by "logSerial"
 */
void CommandHandler::setLogsEnabled(bool enable){
	logEnabled = enable;
}


/**
 * Implement this function.
 * MUST return not found when no valid command was found or if a help command or similar was parsed.
 * When it returns OK or FAIL parsing is normally stopped after this class and the command is not sent to others.
 * A command can not start with "!" or contain ">" anywhere in the reply or command name.
 */
ParseStatus CommandHandler::command(ParsedCommand* cmd,std::string* reply){
	if(!this->commandsEnabled){
		return ParseStatus::NOT_FOUND;
	}
	return ParseStatus::NOT_FOUND;
}


CommandReply CommandHandler::command(ParsedCommand& cmd){
	CommandReply reply;
	reply.type = CommmandReplyType::NOT_FOUND;
	if(!this->commandsEnabled){
		return reply;
	}
	return reply;
}

/**
 * Sends a formatted reply without being prompted by a command.
 * Useful for sending periodic data or with a large delay to a listener on the PC
 */
void CommandHandler::sendSerial(std::string cmd,std::string string, char prefix){
	if(!tud_ready())
		return;
	std::string reply = ">";
	if(prefix){
		std::string t;
		t.assign(1,prefix); // Workaround for chars
		reply += t + ".";
	}
	reply += cmd + ":" + string + "\n";

	tud_cdc_n_write(0,reply.c_str(), reply.length());
	tud_cdc_n_write_flush(0);
}

/**
 * Sends log info back
 */
void CommandHandler::logSerial(std::string string){
	if(!tud_ready() || !logEnabled)
		return;
	std::string reply = ">!" + string + "\n";
	tud_cdc_n_write(0,reply.c_str(), reply.length());
	tud_cdc_n_write_flush(0);
}


std::string CommandHandler::getHelpstring(){
	return std::string(this->getInfo().name) + ":No help\n";
}

void CommandHandler::addCommandHandler(){
	cmdHandlerListMutex.Lock();
	while(this->commandHandlerID != 0xffff){
		auto res = CommandHandler::cmdHandlerIDs.insert(this->commandHandlerID);
		if(res.second){ // Element was inserted
			break;
		}
		this->commandHandlerID++; // Try next id
	}
	addCallbackHandler(cmdHandlers, this);
	cmdHandlerListMutex.Unlock();
}

void CommandHandler::removeCommandHandler(){
	cmdHandlerListMutex.Lock();
	cmdHandlerIDs.erase(this->commandHandlerID); // removes id from list of reserved ids
	removeCallbackHandler(cmdHandlers, this);
	cmdHandlerListMutex.Unlock();
}
