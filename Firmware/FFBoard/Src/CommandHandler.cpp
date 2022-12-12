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
//#include <set>
#include "ChoosableClass.h"

//std::vector<CommandHandler*> CommandHandler::cmdHandlers;
//std::set<uint16_t> CommandHandler::cmdHandlerIDs;
//std::vector<uint16_t> CommandHandler::cmdHandlerIDs;
//cpp_freertos::MutexStandard CommandHandler::cmdHandlerListMutex;
bool CommandHandler::logEnabled = true; // If logs are sent by default

/**
 * clsname and clsid identify this class in commands additionally to the unique instance field which can be assigned at runtime
 */
CommandHandler::CommandHandler(const char* clsname,uint16_t clsid,uint8_t instance) : cmdHandlerInfo({clsname,clsid,instance,0}) {
	addCommandHandler();
}


CommandHandler::~CommandHandler() {
	// Remove from global list when deleted
	removeCommandHandler();
}

/**
 * Generates a readable list of all commands with help information
 * Warning: Large string returned
 */
std::string CommandHandler::getCommandsHelpstring(){
	std::string helpstring = "\n";
	helpstring.reserve(this->registeredCommands.size() * 40); // Helpstrings are large. reserve a big buffer to save on reallocations
	ClassIdentifier info = this->getInfo();
	if(info.name == nullptr || cmdHandlerInfo.clsname == nullptr){
		return "";
	}
	helpstring.append(info.name);
	helpstring.push_back('(');
	helpstring.append(cmdHandlerInfo.clsname);
	helpstring.push_back('.');
	helpstring.append(std::to_string(cmdHandlerInfo.instance)).append("):\n");

	std::string handlerHelp = getHelpstring();

	if(!handlerHelp.empty()){
		helpstring.append(handlerHelp).append("\n");
	}

	if(registeredCommands.empty()){
		helpstring += "No commands.";
	}else{
		helpstring += "cmd\tflags\tdescription\n\n";
		for(CmdHandlerCommanddef& cmd : registeredCommands){
			if(cmd.helpstring != nullptr && cmd.cmd != nullptr){
				helpstring += std::string(cmd.cmd);
				helpstring += "\t(";

				if(cmd.flags & CMDFLAG_GET){
					helpstring+= " R";
				}
				if(cmd.flags & CMDFLAG_SET){
					helpstring+= " W";
				}
				if(cmd.flags & CMDFLAG_SETADR){
					helpstring+= " WA";
				}
				if(cmd.flags & CMDFLAG_GETADR){
					helpstring+= " RA";
				}
				if(cmd.flags & CMDFLAG_INFOSTRING){
					helpstring+= " I";
				}
				if(cmd.flags & CMDFLAG_STR_ONLY){
					helpstring+= " STR";
				}
				if(cmd.flags & CMDFLAG_DEBUG){
					helpstring+= " DBG";
				}
				helpstring.append(" )\t").append(std::string(cmd.helpstring)).append("\n");
			}

		}
	}

	return helpstring;
}

/**
 * Generates a csv list of all commands with help information
 * Warning: Large string returned
 */
std::string CommandHandler::getCsvHelpstring(){
	std::string helpstring = "\nPrefix,Class ID, Class description\n";
	helpstring.reserve(this->registeredCommands.size() * 40); // Helpstrings are large. reserve a big buffer to save on reallocations
	ClassIdentifier info = this->getInfo();
	if(info.name == nullptr || cmdHandlerInfo.clsname == nullptr){
		return "";
	}

	helpstring.append(cmdHandlerInfo.clsname);
	helpstring += '.';
	helpstring += std::to_string(cmdHandlerInfo.instance);
	helpstring += ',';
	char clshex[7];
	std::snprintf(clshex,7,"0x%X",cmdHandlerInfo.clsTypeid);
	helpstring += std::string(clshex);
	helpstring += ',';

	helpstring.append(info.name);

	std::string handlerHelp = getHelpstring();

	if(!handlerHelp.empty()){
		helpstring.append(": ").append(handlerHelp).append("\n");
	}else{
		helpstring += "\n";
	}

	if(registeredCommands.empty()){
		helpstring += "No commands.";
	}else{
		helpstring += "Command name,CMD ID, Description, Flags\n";
		for(CmdHandlerCommanddef& cmd : registeredCommands){
			if(cmd.helpstring != nullptr && cmd.cmd != nullptr){
				char cmdhex[11];
				std::snprintf(cmdhex,11,"0x%lX",cmd.cmdId);
				helpstring.append(cmd.cmd);
				helpstring += ',';
				helpstring += std::string(cmdhex);
				helpstring += ',';
				helpstring.append(cmd.helpstring);
				helpstring += ',';
				if(cmd.flags & CMDFLAG_GET){
					helpstring+= " R";
				}
				if(cmd.flags & CMDFLAG_SET){
					helpstring+= " W";
				}
				if(cmd.flags & CMDFLAG_SETADR){
					helpstring+= " WA";
				}
				if(cmd.flags & CMDFLAG_GETADR){
					helpstring+= " RA";
				}
				if(cmd.flags & CMDFLAG_INFOSTRING){
					helpstring+= " I";
				}
				if(cmd.flags & CMDFLAG_STR_ONLY){
					helpstring+= " (STR)";
				}
				if(cmd.flags & CMDFLAG_DEBUG){
					helpstring+= " (DEBUG)";
				}
				helpstring += "\n";
			}
		}
	}

	return helpstring;
}

/**
 * Registers commonly used internal commands to identify a class
 */
void CommandHandler::registerCommands(){
	registerCommand("id", CommandHandlerCommands::id, "ID of class",CMDFLAG_GET);
	registerCommand("name", CommandHandlerCommands::name, "name of class",CMDFLAG_GET|CMDFLAG_STR_ONLY);
	registerCommand("help", CommandHandlerCommands::help, "Prints help for commands",CMDFLAG_GET | CMDFLAG_STR_ONLY | CMDFLAG_INFOSTRING);
	registerCommand("cmduid", CommandHandlerCommands::cmdhandleruid, "Command handler index",CMDFLAG_GET);
	registerCommand("instance", CommandHandlerCommands::instance, "Command handler instance number",CMDFLAG_GET);
	registerCommand("cmdinfo", CommandHandlerCommands::cmdinfo, "Flags of a command id (adr). -1 if cmd id invalid",CMDFLAG_GETADR);
	//registerCommand("selId", CommandHandlerCommands::selectionid, "Selection id used to create this class",CMDFLAG_GET);
}

/**
 * Returns the ID of a command from a string
 * Ignores commands that match ignoredFlags
 */
CmdHandlerCommanddef* CommandHandler::getCommandFromName(const std::string& cmd,uint32_t ignoredFlags){
	for(CmdHandlerCommanddef& cmdItem : registeredCommands){
		if(cmdItem.cmd == cmd && !(cmdItem.flags & ignoredFlags) && (SystemCommands::debugMode || !(cmdItem.flags & CMDFLAG_DEBUG))){
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
		if(cmdItem.cmdId == id && !(cmdItem.flags & ignoredFlags) && (SystemCommands::debugMode || !(cmdItem.flags & CMDFLAG_DEBUG))){
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
CommandStatus CommandHandler::internalCommand(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	CommandStatus result = CommandStatus::OK;
	switch(static_cast<CommandHandlerCommands>(cmd.cmdId)){

		case CommandHandlerCommands::id:
			replies.emplace_back(this->getInfo().id);
		break;

		case CommandHandlerCommands::name:
			replies.emplace_back(this->getInfo().name);
		break;

		case CommandHandlerCommands::help:
			if(cmd.type == CMDtype::info){
				replies.emplace_back(this->getCsvHelpstring());
			}else{
				replies.emplace_back(this->getCommandsHelpstring());
			}
		break;

		case CommandHandlerCommands::cmdhandleruid:
			replies.emplace_back(this->getCommandHandlerID());
		break;

		case CommandHandlerCommands::instance:
			replies.emplace_back(this->getCommandHandlerInfo()->instance);
		break;

		case CommandHandlerCommands::cmdinfo:
		{
			CmdHandlerCommanddef* command_p = this->getCommandFromId(cmd.adr);
			replies.emplace_back(command_p ? command_p->flags : (int64_t)-1); // Replies with the commands flags if command is valid else with -1
		}
		break;

//		case CommandHandlerCommands::selectionid:
//			replies.emplace_back(this->getSelectionID());
//		break;

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

	for(CommandHandler* cls : getCommandHandlers()){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(strcmp(cmdhandlerinfo->clsname , name) == 0){
			return cmdhandlerinfo->clsTypeid;
		}
	}
	return 0;
}

/**
 * Returns a pointer to the name of a class corresponding to an id or nullptr if not found
 */
const char* CommandHandler::getClassNameFromId(const uint32_t id){

	for(CommandHandler* cls : getCommandHandlers()){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(cmdhandlerinfo->clsTypeid == id){
			return cmdhandlerinfo->clsname;
		}
	}
	return nullptr;
}

/**
 * Returns a command handler with a specific unique handler id or nullptr if not found
 */
CommandHandler* CommandHandler::getHandlerFromHandlerId(const uint16_t cmdhandlerID){
	for(CommandHandler* cls : getCommandHandlers()){
		if(cls->getCommandHandlerID() == cmdhandlerID){
			return cls;
		}
	}
	return nullptr;
}

/**
 * Returns a command handler which matches the class id and instance number or nullptr if not found
 */
CommandHandler* CommandHandler::getHandlerFromId(const uint16_t id,const uint8_t instance){

	for(CommandHandler* cls : getCommandHandlers()){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(cmdhandlerinfo->clsTypeid == id && (cmdhandlerinfo->instance == instance || instance == 0xFF)){
			return cls;
		}
	}
	return nullptr;
}

/**
 * Returns a command handler from a classname and instance numer or nullptr if not found
 */
CommandHandler* CommandHandler::getHandlerFromClassName(const char* name,const uint8_t instance){

	for(CommandHandler* cls : getCommandHandlers()){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(strcmp(cmdhandlerinfo->clsname, name) == 0 && (cmdhandlerinfo->instance == instance || instance == 0xFF)){
			return cls;
		}
	}
	return nullptr;
}

/**
 * Returns all command handlers with a supplied classname
 */
std::vector<CommandHandler*> CommandHandler::getHandlersFromClassName(const char* name){
	std::vector<CommandHandler*> reply;
	for(CommandHandler* cls : getCommandHandlers()){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(strcmp(cmdhandlerinfo->clsname, name) == 0){
			reply.push_back(cls);
		}
	}
	return reply;
}
/**
 * Returns all command handlers with a supplied class id
 */
std::vector<CommandHandler*> CommandHandler::getHandlersFromId(const uint16_t id){
	std::vector<CommandHandler*> reply;
	for(CommandHandler* cls : getCommandHandlers()){
		CmdHandlerInfo* cmdhandlerinfo = cls->getCommandHandlerInfo();
		if(id == cmdhandlerinfo->clsTypeid){
			reply.push_back(cls);
		}
	}
	return reply;
}

/**
 * Returns true if a command handler pointer is in the list of active command handlers
 */
bool CommandHandler::isInHandlerList(CommandHandler* handler){
	return std::find(getCommandHandlers().begin(), getCommandHandlers().end(), handler) != getCommandHandlers().end();
}

/**
 * Returns true if the command id is valid for this command handler and does NOT contain ignoredFlags but contains all requiredFlags
 */
bool CommandHandler::isValidCommandId(uint32_t cmdid,uint32_t ignoredFlags,uint32_t requiredFlags){
	for(CmdHandlerCommanddef& cmd : registeredCommands){
		if(cmd.cmdId == cmdid && !(cmd.flags & ignoredFlags) && ((cmd.flags & requiredFlags) == requiredFlags) && (SystemCommands::debugMode || !(cmd.flags & CMDFLAG_DEBUG))){
			return true;
		}
	}
	return false;
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

	return reply;
}

/**
 * Broadcasts an unrequested reply to all command interfaces in the name of this class
 * @param[in]	reply	a command reply to be sent
 * @param[in]	cmdId	the command id in which name to reply
 * @param[in]	type	type of the command to create (normally get)
 */
void CommandHandler::broadcastCommandReply(CommandReply reply, uint32_t cmdId,CMDtype type){
	std::vector<CommandReply> replies = {reply};
	CommandInterface::broadcastCommandReplyAsync(replies, this, cmdId, type);
}

/**
 * Broadcasts an unrequested reply to a specific command interface in the name of this class
 * @param[in]	interface	Must be a valid interface pointer from a previously received command or nullptr (treated like a broadcast)
 * @param[in]	reply	a command reply to be sent
 * @param[in]	cmdId	the command id in which name to reply
 * @param[in]	type	type of the command to create (normally get)
 */
void CommandHandler::sendCommandReplyAsync(CommandReply reply, uint32_t cmdId,CMDtype type,CommandInterface* interface){
	if(interface == nullptr){
		broadcastCommandReply(reply, cmdId, type);
		return;
	}

	std::vector<CommandReply> replies = {reply};
	interface->sendReplyAsync(replies, this, cmdId, type);
}


/**
 * Sends log info back via cdc. UNUSED
 */
void CommandHandler::logSerial(std::string string){
	if( !logEnabled) // !tud_ready() ||
		return;
	std::string reply = "[>!" + string + "\n]";
	CDCcomm::cdcSend(&reply, 0);

}

/**
 * Sends log info back via cdc if debug mode is on
 */
void CommandHandler::logSerialDebug(std::string string)
{
	if(SystemCommands::debugMode){
		logSerial(string);
	}
}

/**
 * Returns a description of this class
 */
std::string CommandHandler::getHelpstring(){
	return "";
}

/**
 * Returns a pointer to this classes command handler info struct containing its name and ids
 */
CmdHandlerInfo* CommandHandler::getCommandHandlerInfo(){
	return &cmdHandlerInfo;
}

/**
 * Returns the instance number of this class
 */
uint8_t CommandHandler::getCommandHandlerInstance(){
	return cmdHandlerInfo.instance;
}

/**
 * Registers a command handler in the global callback list and assigns a unique index number
 */
void CommandHandler::addCommandHandler(){
	//cmdHandlerListMutex.Lock();
//	while(this->cmdHandlerInfo.commandHandlerID != 0xffff){
//		auto res = CommandHandler::cmdHandlerIDs.insert(cmdHandlerIDs.end(),this->cmdHandlerInfo.commandHandlerID);
//		if(res.second){ // Element was inserted
//			break;
//		}
//		this->cmdHandlerInfo.commandHandlerID++; // Try next id
//	}
	addCallbackHandler(getCommandHandlers(), this);
	std::vector<uint16_t>&cmdHandlerIDs = getCommandHandlerIds();
	while(this->cmdHandlerInfo.commandHandlerID++ != 0xffff){
		if(std::find(cmdHandlerIDs.begin(),cmdHandlerIDs.end(), cmdHandlerInfo.commandHandlerID) == cmdHandlerIDs.end()){
			cmdHandlerIDs.push_back(this->cmdHandlerInfo.commandHandlerID);
			pulseSysLed();
			break;
		}
	}



	//cmdHandlerListMutex.Unlock();
}

/**
 * Removes a class from the global callback list and frees its id
 */
void CommandHandler::removeCommandHandler(){
	//cmdHandlerListMutex.Lock();
	//cmdHandlerIDs.erase(this->cmdHandlerInfo.commandHandlerID); // removes id from list of reserved ids
	std::vector<uint16_t>&cmdHandlerIDs = getCommandHandlerIds();
	cmdHandlerIDs.erase(std::find(cmdHandlerIDs.begin(),cmdHandlerIDs.end(), cmdHandlerInfo.commandHandlerID)); // removes id from list of reserved ids
	removeCallbackHandler(getCommandHandlers(), this);
	//cmdHandlerListMutex.Unlock();
}
