/*
 * CommandHandler.h
 *
 *  Created on: 03.04.2020
 *      Author: Yannick
 */

#ifndef COMMANDHANDLER_H_
#define COMMANDHANDLER_H_
//#include <CmdParser.h>
#include "ChoosableClass.h"
#include <set>
#include "mutex.hpp"
#include "ClassIDs.h"
#include <vector>
//#include "CommandInterface.h"


#define CMDFLAG_GET	 	0x01
#define CMDFLAG_SET	 	0x02
#define CMDFLAG_INFOSTRING	 0x08
#define CMDFLAG_GETADR	 	0x10
#define CMDFLAG_SETADR	 	0x20
#define CMDFLAG_HIDDEN	 0x40
#define CMDFLAG_DEBUG	 0x80

#define CMDFLAG_STR_ONLY 0x100
#define CMDFLAG_HID_ONLY 0x200 // Command not available for string based parsers

enum class CMDtype : uint32_t{
			none = 0,
			set = CMDFLAG_SET,
			setat = CMDFLAG_SETADR,
			get = CMDFLAG_GET,
			getat = CMDFLAG_GETADR,
			info = CMDFLAG_INFOSTRING,
			err = 0x1000
};

enum class CommandStatus : uint8_t {NOT_FOUND,OK,ERR,NO_REPLY,BROADCAST};
enum class CommandReplyType : uint8_t {NONE,ACK,INT,STRING,STRING_OR_INT,STRING_OR_DOUBLEINT,DOUBLEINTS};

class CommandInterface;
class CommandHandler; // defined lower

struct CmdHandlerCommanddef
{
	const char* cmd = nullptr;
	const char* helpstring = nullptr;
	const uint32_t cmdId;
	const uint32_t flags;
};

struct CmdHandlerInfo
{
	const char* clsname = nullptr;
	const uint16_t clsTypeid;
	uint8_t instance;
	uint16_t commandHandlerID = 1;
};


struct ParsedCommand
{
	uint32_t cmdId=0;
    int64_t adr = 0;
    int64_t val = 0;
    uint8_t instance = 0xFF; // instance number. decided by the class. 0xFF if all instances are targeted
    CommandHandler* target = nullptr; // Directly target a handler
    CMDtype type = CMDtype::none;
};


/**
 * Reply object for command handlers
 */
class CommandReply{
public:
	CommandReply(){}; // empty
	CommandReply(CommandReplyType type) : type(type){}; // empty
	CommandReply(int64_t val) : val(val), type(CommandReplyType::INT){};
	CommandReply(int64_t val,int64_t adr) : val(val),adr(adr),type(CommandReplyType::DOUBLEINTS){};
	CommandReply(std::string reply) : reply(reply), type(CommandReplyType::STRING){};
	CommandReply(std::string reply,int64_t val) : reply(reply),val(val), type(CommandReplyType::STRING_OR_INT){};
	CommandReply(std::string reply,int64_t val,int64_t adr) : reply(reply),val(val),adr(adr), type(CommandReplyType::STRING_OR_DOUBLEINT){};

    std::string reply;
    int64_t val = 0;
    int64_t adr = 0;
    CommandReplyType type = CommandReplyType::ACK;
};

struct CommandResult {
	std::vector<CommandReply> reply;
	ParsedCommand originalCommand;
	uint16_t handlerId = 0; //!< ID of the command handler responding to the command
	//CommandInterface* originalInterface = nullptr;
	CommandHandler* commandHandler = nullptr;
	CommandStatus type = CommandStatus::NOT_FOUND;
};

// All commands have the 0x80000000 but set! do no use anywhere else
enum class CommandHandlerCommands : uint32_t{
	id=0x80000001,name=0x80000002,help=0x80000003,instance=0x80000004,cmdhandleruid=0x80000005,selectionid=0x80000006
};

/**
 * Implements an interface for parsed command handlers.
 *
 * Adds itself to a global vector of handlers that can be called from the main class when a command gets parsed.
 *
 * For example motor drivers and button sources can implement this to easily get serial commands
 */
class CommandHandler {
public:
	static std::vector<CommandHandler*> cmdHandlers; //!< List of all registered command handlers to be called on commands
	static std::set<uint16_t> cmdHandlerIDs; //!< Reserves dynamic unique IDs to keep track of command handlers
	static cpp_freertos::MutexStandard cmdHandlerListMutex;
	/**
	 * Type of this class. Mainclass, motordriver...
	 * Should be implemented by the parent class so it is not in the info struct
	 */
	virtual const ClassType getClassType(){return ClassType::NONE;};

	CommandHandler(const char* clsname,uint16_t clsid,uint8_t instance = 0);
	virtual ~CommandHandler();
	virtual bool hasCommands();
	virtual void setCommandsEnabled(bool enable);

	virtual CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands(); // Function reserved to register commands in the command list.
	virtual CommandStatus internalCommand(const ParsedCommand& cmd,std::vector<CommandReply>& replies,CommandInterface* interface = nullptr);

	virtual const ClassIdentifier getInfo() = 0; //!< Command handlers always have class infos. Works well with ChoosableClass

	virtual std::string getHelpstring(); // Returns a help string if "help" command is sent
	virtual std::string getCommandsHelpstring(); // Returns a list of the commands helpstrings
	virtual std::string getCsvHelpstring(); // Returns a list of the commands helpstrings formatted for csv
	//static void sendSerial(std::string cls,std::string cmd,std::string string,uint8_t prefix = 0xFF); //!< Send a command reply formatted sequence
	static void logSerial(std::string string);	//!< Send a log formatted sequence

	void broadcastCommandReply(CommandReply reply, uint32_t cmdId,CMDtype type);

	static bool logEnabled;
	static bool logsEnabled();
	static void setLogsEnabled(bool enabled);
	virtual uint16_t getCommandHandlerID(){return this->cmdHandlerInfo.commandHandlerID;}
	virtual uint16_t getSelectionID(){return this->getInfo().id;} //!< normally returns class id but for choosable classes this can be the id used to create the class

	virtual CmdHandlerInfo* getCommandHandlerInfo();



	static uint32_t getClassIdFromName(const char* name);
	static const char* getClassNameFromId(const uint32_t id);

	static CommandHandler* getHandlerFromHandlerId(const uint16_t cmdhandlerID);
	static CommandHandler* getHandlerFromId(const uint16_t id,const uint8_t instance=0xFF);
	static CommandHandler* getHandlerFromClassName(const char* name,const uint8_t instance=0xFF);
	static std::vector<CommandHandler*> getHandlersFromClassName(const char* name);
	static std::vector<CommandHandler*> getHandlersFromId(const uint16_t id);
	static bool isInHandlerList(CommandHandler* handler);

	static std::string getAllHelpstrings();

	virtual bool isValidCommandId(uint32_t cmdid,uint32_t ignoredFlags=0,uint32_t requiredFlag=0);

	virtual CmdHandlerCommanddef* getCommandFromName(const std::string& cmd,uint32_t ignoredFlags=0);
	virtual CmdHandlerCommanddef* getCommandFromId(const uint32_t id,uint32_t ignoredFlags=0);

protected:
	void setInstance(uint8_t instance);
	bool commandsEnabled = true;
	virtual void addCommandHandler();
	virtual void removeCommandHandler();


	/**
	 * Reads or writes a variable
	 */
	template<typename TVal>
	inline CommandStatus handleGetSet(const ParsedCommand& cmd,std::vector<CommandReply>& replies,TVal& value){
		if(cmd.type == CMDtype::set){
			value = static_cast<TVal>(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(value));
		}else{
			return CommandStatus::ERR;
		}
		return CommandStatus::OK;
	}
	/**
	 * Reads from a variable and passes set commands to a member callback
	 */
	template<typename TVal,class cls>
	inline void handleGetSetFunc(const ParsedCommand& cmd,std::vector<CommandReply>& replies,TVal& value,void (cls::*setfunc)(TVal),cls* obj){
		if(cmd.type == CMDtype::set){
			(obj->*setfunc)(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(value));
		}
	}
	/**
	 * Reads from a member function and sets to a member function
	 */
	template<typename TVal,class cls,class cls1,class cls2>
	inline void handleGetFuncSetFunc(const ParsedCommand& cmd,std::vector<CommandReply>& replies,TVal (cls1::*getfunc)(),void (cls2::*setfunc)(TVal),cls* obj){
		if(cmd.type == CMDtype::set){
			(obj->*setfunc)(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply((obj->*getfunc)()));
		}
	}
	/**
	 * Reads from a member function and writes to a variable
	 */
	template<typename TVal,class cls>
	inline void handleGetFuncSet(const ParsedCommand& cmd,std::vector<CommandReply>& replies,TVal& value,TVal (cls::*getfunc)(),cls* obj){
		if(cmd.type == CMDtype::set){
			value = static_cast<TVal>(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply((obj->*getfunc)()));
		}
	}

	//uint16_t commandHandlerID = 1; // 0 reserved for system


	std::vector<CmdHandlerCommanddef> registeredCommands;
	//void registerCommand(std::string cmd,uint16_t cmdid, std::string help="");
	// Helper to be used with class enums
	template<typename ID>
	void registerCommand(const char* cmd,const ID cmdid,const char* help=nullptr,uint32_t flags = 0){
		for(CmdHandlerCommanddef& cmdDef : registeredCommands){
			if(cmdDef.cmdId == static_cast<uint32_t>(cmdid))
				return; //already present
		}

		CmdHandlerCommanddef cmddef = {
			.cmd=cmd,
			.helpstring = help,
			.cmdId = static_cast<uint32_t>(cmdid),
			.flags = flags
		};
		this->registeredCommands.push_back(cmddef);
		this->registeredCommands.shrink_to_fit();
	}


//	std::string clsname;
//	uint16_t clsid;
//	uint8_t cmdHandlerInstance=0;

	CmdHandlerInfo cmdHandlerInfo;

};

#endif /* COMMANDHANDLER_H_ */
