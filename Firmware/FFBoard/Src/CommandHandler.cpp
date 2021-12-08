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
#include "CDCcomm.h"
#include <set>

std::vector<CommandHandler*> CommandHandler::cmdHandlers;
std::set<uint16_t> CommandHandler::cmdHandlerIDs;
cpp_freertos::MutexStandard CommandHandler::cmdHandlerListMutex;
bool CommandHandler::logEnabled = true; // If logs are sent by default

/**
 * clsname and clsid identify this class in commands additionally to the unique instance field which can be assigned at runtime
 */
CommandHandler::CommandHandler(const char* clsname,uint16_t clsid,uint8_t instance) : cmdHandlerInfo({clsname,clsid,instance}) {
	addCommandHandler();
}


CommandHandler::~CommandHandler() {
	// Remove from global list when deleted
	removeCommandHandler();
}


std::string CommandHandler::getCommandsHelpstring(){
	std::string helpstring;
	ClassIdentifier info = this->getInfo();
	if(info.name == nullptr || cmdHandlerInfo.clsname == nullptr){
		return "";
	}
	helpstring.append(info.name);
	helpstring +=  "(";
	helpstring.append(cmdHandlerInfo.clsname);
	helpstring += "." + std::to_string(cmdHandlerInfo.instance) + "):\n";
	std::string handlerHelp = getHelpstring();

	if(!handlerHelp.empty()){
		helpstring += handlerHelp + "\n";
	}

	if(registeredCommands.empty()){
		helpstring += "No commands.";
	}else{
		helpstring += "Commands with help:\n";
		for(CmdHandlerCommanddef& cmd : registeredCommands){
			if(cmd.helpstring != nullptr && cmd.cmd != nullptr){
				helpstring.append(cmd.cmd);
				helpstring += "," + std::to_string(cmd.cmdId) + ",";
				helpstring.append(cmd.helpstring);
				helpstring += "\n";
			}
				//helpstring += cmd.cmd + ":" + cmd.helpstring+"\n";
		}
	}

	return helpstring;
}

void CommandHandler::registerCommands(){
	registerCommand("id", CommandHandlerCommands::id, "ID of class");
	registerCommand("name", CommandHandlerCommands::name, "name of class",CMDFLAG_STR_ONLY);
	registerCommand("help", CommandHandlerCommands::help, "Prints help for commands",CMDFLAG_STR_ONLY);
	registerCommand("cmduid", CommandHandlerCommands::cmdhandleruid, "Command handler index");
	registerCommand("instance", CommandHandlerCommands::instance, "Command handler instance number");
	registerCommand("selId", CommandHandlerCommands::selectionid, "Selection id used to create this class");
}

/**
 * Returns the ID of a command from a string
 * Ignores commands that match ignoredFlags
 */
CmdHandlerCommanddef* CommandHandler::getCommandFromName(const std::string& cmd,uint32_t ignoredFlags){
	for(CmdHandlerCommanddef& cmdItem : registeredCommands){
		if(cmdItem.cmd == cmd && !(cmdItem.flags & ignoredFlags)){
			return &cmdItem;
		}
	}
	return nullptr;
}

/**
 * Returns the string name of a command from an ID
 * Ignores commands that match ignoredFlags
 */
CmdHandlerCommanddef* CommandHandler::getCommandFromId(const uint32_t id,uint32_t ignoredFlags){
	for(CmdHandlerCommanddef& cmdItem : registeredCommands){
		if(cmdItem.cmdId == id && !(cmdItem.flags & ignoredFlags)){
			return &cmdItem;
		}
	}
	return nullptr;
}

/**
 * Changes the command handler instance number for addressing.
 * Should only be called in the constructor
 */
void CommandHandler::setInstance(uint8_t instance){
	this->cmdHandlerInfo.instance = instance;
}

/**
 * Some standard commands most classes will need
 */
CommandStatus CommandHandler::internalCommand(const ParsedCommand& cmd,std::vector<CommandReply>& replies,CommandInterface* interface){
	CommandStatus result = CommandStatus::OK;
	switch(static_cast<CommandHandlerCommands>(cmd.cmdId)){

		case CommandHandlerCommands::id:
			replies.push_back(CommandReply(this->getInfo().id));
		break;

		case CommandHandlerCommands::name:
			replies.push_back(CommandReply(this->getInfo().name));
		break;

		case CommandHandlerCommands::help:
			replies.push_back(CommandReply(this->getCommandsHelpstring()));
		break;

		case CommandHandlerCommands::cmdhandleruid:
			replies.push_back(CommandReply(this->getCommandHandlerID()));
		break;

		case CommandHandlerCommands::instance:
			replies.push_back(CommandReply(this->getCommandHandlerInfo()->instance));
		break;

		case CommandHandlerCommands::selectionid:
			replies.push_back(CommandReply(this->getSelectionID()));
		break;

		default:
			result = CommandStatus::NOT_FOUND;
		break;
	}
	return result;
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
 * Searches cmd handlers and returns its class id if the classname matches
 * To be used to convert from string commands to ids
 */
uint32_t CommandHandler::getClassIdFromName(const char* name){

	for(CommandHandler* cls : cmdHandlers){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(strcmp(cmdhandlerinfo->clsname , name) == 0){
			return cmdhandlerinfo->clsTypeid;
		}
	}
	return 0;
}
const std::string CommandHandler::getClassNameFromId(const uint32_t id){

	for(CommandHandler* cls : cmdHandlers){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(cmdhandlerinfo->clsTypeid == id){
			return cmdhandlerinfo->clsname;
		}
	}
	return nullptr;
}
CommandHandler* CommandHandler::getHandlerFromHandlerId(const uint16_t cmdhandlerID){
	for(CommandHandler* cls : cmdHandlers){
		if(cls->getCommandHandlerID() == cmdhandlerID){
			return cls;
		}
	}
	return nullptr;
}

CommandHandler* CommandHandler::getHandlerFromId(const uint16_t id,const uint8_t instance){

	for(CommandHandler* cls : cmdHandlers){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(cmdhandlerinfo->clsTypeid == id && (cmdhandlerinfo->instance == instance || instance == 0xFF)){
			return cls;
		}
	}
	return nullptr;
}

CommandHandler* CommandHandler::getHandlerFromClassName(const char* name,const uint8_t instance){

	for(CommandHandler* cls : cmdHandlers){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(strcmp(cmdhandlerinfo->clsname, name) == 0 && (cmdhandlerinfo->instance == instance || instance == 0xFF)){
			return cls;
		}
	}
	return nullptr;
}
std::vector<CommandHandler*> CommandHandler::getHandlersFromClassName(const char* name){
	std::vector<CommandHandler*> reply;
	for(CommandHandler* cls : cmdHandlers){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(strcmp(cmdhandlerinfo->clsname, name) == 0){
			reply.push_back(cls);
		}
	}
	return reply;
}

bool CommandHandler::isInHandlerList(CommandHandler* handler){
	return std::find(cmdHandlers.begin(), cmdHandlers.end(), handler) != cmdHandlers.end();
}

bool CommandHandler::isValidCommandId(uint32_t cmdid,uint32_t ignoredFlags,uint32_t requiredFlags){
	for(CmdHandlerCommanddef& cmd : registeredCommands){
		if(cmd.cmdId == cmdid && !(cmd.flags & ignoredFlags) && ((cmd.flags & requiredFlags) == requiredFlags)){
			return true;
		}
	}
	return false;
}

std::string CommandHandler::getAllHelpstrings(){
	std::string helpstring = "";
	for(CommandHandler* handler : cmdHandlers){
		helpstring += handler->getCommandsHelpstring();
		helpstring += "\n";
	}
	return helpstring;
}



/**
 * @param[in] 	cmd 		The parsed command to be executed.
 * @param[out] 	replies 	A vector to return one or multiple reply objects into.
 * Replies to the interface will be generated based on the reply objects.
 * A string reply may not contain start, end and separation markers: [,],|
 * Other characters are allowed.
 */
CommandStatus CommandHandler::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	CommandStatus reply=CommandStatus::NOT_FOUND;
//	ParsedCommand_old oldcmd = {
//
//			.cmd = cmd.cmd,//getCommandNameFromId(cmd.cmdId)
//			.adr = cmd.adr,
//			.val = cmd.val,
//			.prefix = cmd.instance.has_value() ? (char)(cmd.instance.value()+'0') : '\0',
//			.type = cmd.type
//
//	};
	return reply;
}

/**
 * Sends a formatted reply without being prompted by a command.
 * Useful for sending periodic data or with a large delay to a listener on the PC
 */
void CommandHandler::sendSerial(std::string cls,std::string cmd,std::string string, uint8_t prefix){
//	if(!tud_ready())
//		return;
	std::string reply = "[" + cls + ".";
	if(prefix!=0xFF){
		reply += std::to_string(prefix) + ".";
	}
	reply += cmd + "|" + string + "]\n";

	CDCcomm::cdcSend(&reply, 0);
//	tud_cdc_n_write(0,reply.c_str(), reply.length());
//	tud_cdc_n_write_flush(0);
}

/**
 * Sends log info back
 */
void CommandHandler::logSerial(std::string string){
	if( !logEnabled) // !tud_ready() ||
		return;
	std::string reply = ">!" + string + "\n";
	CDCcomm::cdcSend(&reply, 0);
//	tud_cdc_n_write(0,reply.c_str(), reply.length());
//	tud_cdc_n_write_flush(0);
}


std::string CommandHandler::getHelpstring(){
	return "";
}

CmdHandlerInfo* CommandHandler::getCommandHandlerInfo(){
	return &cmdHandlerInfo;
}

void CommandHandler::addCommandHandler(){
	cmdHandlerListMutex.Lock();
	while(this->cmdHandlerInfo.commandHandlerID != 0xffff){
		auto res = CommandHandler::cmdHandlerIDs.insert(this->cmdHandlerInfo.commandHandlerID);
		if(res.second){ // Element was inserted
			break;
		}
		this->cmdHandlerInfo.commandHandlerID++; // Try next id
	}
	addCallbackHandler(cmdHandlers, this);
	cmdHandlerListMutex.Unlock();
}

void CommandHandler::removeCommandHandler(){
	cmdHandlerListMutex.Lock();
	cmdHandlerIDs.erase(this->cmdHandlerInfo.commandHandlerID); // removes id from list of reserved ids
	removeCallbackHandler(cmdHandlers, this);
	cmdHandlerListMutex.Unlock();
}
