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

std::vector<CommandHandler*> CommandHandler::cmdHandlers;
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

/*
 * Enables or disables logs sent by "logSerial"
 */
void CommandHandler::setLogsEnabled(bool enable){
	logEnabled = enable;
}


/*
 * Implement this function
 * MUST return not found when no valid command was found or if a help command or similar was parsed
 * When it returns OK or FAIL parsing is normally stopped after this class and the command is not sent to others
 * A command can not start with "!" or contain ">" anywhere in the reply or command name.
 */
ParseStatus CommandHandler::command(ParsedCommand* cmd,std::string* reply){
	if(!this->commandsEnabled){
		return ParseStatus::NOT_FOUND;
	}
	return ParseStatus::NOT_FOUND;
}

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

/*
 * Sends log info
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
	addCallbackHandler(cmdHandlers, this);
}

void CommandHandler::removeCommandHandler(){
	removeCallbackHandler(cmdHandlers, this);
}
