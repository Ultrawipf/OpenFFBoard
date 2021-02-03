/*
 * CommandHandler.cpp
 *
 *  Created on: 03.04.2020
 *      Author: Yannick
 */

#include "CommandHandler.h"
#include "global_callbacks.h"
#include "FFBoardMain.h"

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


/*
 * Implement this function
 * MUST return not found when no valid command was found or if a help command or similar was parsed
 * When it returns OK or FAIL parsing is normally stopped after this class and the command is not sent to others
 */
ParseStatus CommandHandler::command(ParsedCommand* cmd,std::string* reply){
	if(!this->commandsEnabled){
		return ParseStatus::NOT_FOUND;
	}
	return ParseStatus::NOT_FOUND;
}


std::string CommandHandler::getHelpstring(){
	return std::string(this->getInfo().name) + ":No help\n";
}

void CommandHandler::addCommandHandler(){
	// If already added return
	extern std::vector<CommandHandler*> cmdHandlers;
	addCallbackHandler(&cmdHandlers, this);
}

void CommandHandler::removeCommandHandler(){
	extern std::vector<CommandHandler*> cmdHandlers;
	removeCallbackHandler(&cmdHandlers, this);
}
