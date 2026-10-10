/*
 * MockCommandHandler.h
 *
 * A command handler with one command for every access type and flag of the command system.
 * It registers itself in the global command handler list like any firmware class,
 * records every command it receives and replies with configurable data.
 *
 * Use it as the target for command interface and parser tests instead of real firmware classes.
 */
#ifndef HOSTTEST_MOCKCOMMANDHANDLER_H_
#define HOSTTEST_MOCKCOMMANDHANDLER_H_

#include "CommandHandler.h"
#include <map>
#include <string>
#include <vector>

/**
 * Command ids of the mock. The string command names are identical to the enumerator names.
 */
enum class MockCmd : uint32_t {
	value = 0,	  //!< get/set of an integer value
	addr = 1,	  //!< get/set of values at an address (getat/setat)
	readonly = 2, //!< get only. Replies with `value`
	writeonly = 3,//!< set only. Writes `value`
	info = 4,	  //!< get, set and info string
	str = 5,	  //!< get. Replies with a string only
	stronly = 6,  //!< get. Only available for string interfaces (CMDFLAG_STR_ONLY)
	hidonly = 7,  //!< get. Not available for string interfaces (CMDFLAG_HID_ONLY)
	debug = 8,	  //!< get. Only available in debug mode (CMDFLAG_DEBUG)
	multi = 9,	  //!< get. Replies with 3 integer replies
	noreply = 10, //!< get. Returns CommandStatus::NO_REPLY
	fail = 11,	  //!< get/set. Returns CommandStatus::ERR
	untyped = 12, //!< No access flags. Accepts every command type
	dual = 13,	  //!< get. Replies with a value and address pair
	strint = 14,  //!< get. Replies with string and int (STRING_OR_INT)
	ack = 15,	  //!< get. Returns OK without any reply
	unknown = 0xFFFF //!< Never registered
};

class MockCommandHandler : public CommandHandler {
public:
	MockCommandHandler(const char *clsname = "mock", uint16_t clsid = CLSID_CUSTOM, uint8_t instance = 0)
		: CommandHandler(clsname, clsid, instance), clsid(clsid) {
		CommandHandler::registerCommands(); // id, name, help, cmduid, instance, cmdinfo
		registerCommand("value", MockCmd::value, "Integer value", CMDFLAG_GET | CMDFLAG_SET);
		registerCommand("addr", MockCmd::addr, "Value at address", CMDFLAG_GETADR | CMDFLAG_SETADR);
		registerCommand("readonly", MockCmd::readonly, "Read only value", CMDFLAG_GET);
		registerCommand("writeonly", MockCmd::writeonly, "Write only value", CMDFLAG_SET);
		registerCommand("info", MockCmd::info, "Value with info string", CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
		registerCommand("str", MockCmd::str, "String reply", CMDFLAG_GET);
		registerCommand("stronly", MockCmd::stronly, "String interface only", CMDFLAG_GET | CMDFLAG_STR_ONLY);
		registerCommand("hidonly", MockCmd::hidonly, "HID interface only", CMDFLAG_GET | CMDFLAG_HID_ONLY);
		registerCommand("debug", MockCmd::debug, "Debug only", CMDFLAG_GET | CMDFLAG_DEBUG);
		registerCommand("multi", MockCmd::multi, "Multiple replies", CMDFLAG_GET);
		registerCommand("noreply", MockCmd::noreply, "No reply", CMDFLAG_GET);
		registerCommand("fail", MockCmd::fail, "Always fails", CMDFLAG_GET | CMDFLAG_SET);
		registerCommand("untyped", MockCmd::untyped, "No access flags", 0);
		registerCommand("dual", MockCmd::dual, "Value and address reply", CMDFLAG_GET);
		registerCommand("strint", MockCmd::strint, "String or int reply", CMDFLAG_GET);
		registerCommand("ack", MockCmd::ack, "Empty reply", CMDFLAG_GET);
	}

	const ClassIdentifier getInfo() override { return ClassIdentifier{"Mock handler", clsid, ClassVisibility::visible}; }
	std::string getHelpstring() override { return "Mock command handler"; }

	CommandStatus command(const ParsedCommand &cmd, std::vector<CommandReply> &replies) override {
		received.push_back(cmd);
		switch (static_cast<MockCmd>(cmd.cmdId)) {
		case MockCmd::value:
		case MockCmd::readonly:
		case MockCmd::writeonly:
		case MockCmd::untyped:
			return handleGetSet(cmd, replies, value);
		case MockCmd::addr:
			if (cmd.type == CMDtype::setat) {
				values[cmd.adr] = cmd.val;
			} else if (cmd.type == CMDtype::getat) {
				replies.emplace_back(values[cmd.adr], cmd.adr);
			} else {
				return CommandStatus::ERR;
			}
			return CommandStatus::OK;
		case MockCmd::info:
			if (cmd.type == CMDtype::info) {
				replies.emplace_back(std::string("info text"));
				return CommandStatus::OK;
			}
			return handleGetSet(cmd, replies, value);
		case MockCmd::str:
		case MockCmd::stronly:
			replies.emplace_back(stringReply);
			return CommandStatus::OK;
		case MockCmd::hidonly:
		case MockCmd::debug:
			replies.emplace_back(value);
			return CommandStatus::OK;
		case MockCmd::multi:
			replies.emplace_back((int64_t)1);
			replies.emplace_back((int64_t)2);
			replies.emplace_back((int64_t)3);
			return CommandStatus::OK;
		case MockCmd::noreply:
			return CommandStatus::NO_REPLY;
		case MockCmd::fail:
			return CommandStatus::ERR;
		case MockCmd::dual:
			replies.emplace_back(value, (int64_t)dualAdr);
			return CommandStatus::OK;
		case MockCmd::strint:
			replies.emplace_back(stringReply, value);
			return CommandStatus::OK;
		case MockCmd::ack:
			return CommandStatus::OK;
		default:
			return CommandStatus::NOT_FOUND;
		}
	}

	/** Returns the last received command. Fails hard if none was received */
	const ParsedCommand &last() const { return received.at(received.size() - 1); }

	static uint32_t id(MockCmd cmd) { return static_cast<uint32_t>(cmd); }

	// --- State ---
	std::vector<ParsedCommand> received; //!< Commands passed to command() (not the internal commands like id/help)
	int64_t value = 0;
	std::map<int64_t, int64_t> values;
	int64_t dualAdr = 0;
	std::string stringReply = "text";

private:
	uint16_t clsid;
};

#endif
