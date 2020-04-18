/*
 * CommandHandler.h
 *
 *  Created on: 03.04.2020
 *      Author: Yannick
 */

#ifndef COMMANDHANDLER_H_
#define COMMANDHANDLER_H_
#include <CmdParser.h>

/*
 * Implements an interface for parsed command handlers
 * Adds itself to a global vector of handlers that can be called from the main class when a command gets parsed
 * For example motor drivers and button sources can implement this to easily get serial commands
 */
class CommandHandler {
public:
	CommandHandler();
	virtual ~CommandHandler();
	virtual bool hasCommands();
	virtual void setCommandsEnabled(bool enable);
	virtual bool command(ParsedCommand* cmd,std::string* reply);
protected:
	bool commandsEnabled = true;
	virtual void addCommandHandler();
	virtual void removeCommandHandler();

private:

};

#endif /* COMMANDHANDLER_H_ */
