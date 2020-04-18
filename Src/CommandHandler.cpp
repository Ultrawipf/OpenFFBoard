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
 * MUST return false when no valid command was found or if a help command or similar was parsed
 * When it returns true parsing is normally stopped after this class and not sent to others
 */
bool CommandHandler::command(ParsedCommand* cmd,std::string* reply){
	if(!this->commandsEnabled){
		return false;
	}
	return false;
}


void CommandHandler::addCommandHandler(){
	// If already added return
	extern std::vector<CommandHandler*> cmdHandlers;
	for(uint8_t i = 0; i < cmdHandlers.size(); i++){
		if(cmdHandlers[i] == this)
			return;
	}
	cmdHandlers.push_back(this);
}

void CommandHandler::removeCommandHandler(){
	extern std::vector<CommandHandler*> cmdHandlers;
	for (uint8_t i = 0; i < cmdHandlers.size(); i++){
		if(cmdHandlers[i] == this){
			cmdHandlers.erase(cmdHandlers.begin()+i);
			break;
		}
	}
}
