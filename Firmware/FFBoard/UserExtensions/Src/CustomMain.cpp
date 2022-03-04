/*
 * exampleMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include <CustomMain.h>

// Change this
ClassIdentifier CustomMain::info = {
		 .name = "Custom" ,
		 .id=CLSID_CUSTOM,
		 .visibility = ClassVisibility::hidden
 };
// Copy this to your class for identification
const ClassIdentifier CustomMain::getInfo(){
	return info;
}



CustomMain::CustomMain() {
	registerCommands(); // Register all the commands
}

CustomMain::~CustomMain() {

}

void CustomMain::registerCommands(){
	CommandHandler::registerCommands(); // Register internal commands for getting the id and help
	registerCommand("command", CustomMain_commands::command, "Controls the example variable");
}

std::string CustomMain::getHelpstring(){
	return "This is a basic example mainclass with commands";
}

CommandStatus CustomMain::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<CustomMain_commands>(cmd.cmdId))
	{

	case CustomMain_commands::command:
		// Sets or gets the variable
		handleGetSet(cmd, replies, this->examplevar);
		break;


	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}
