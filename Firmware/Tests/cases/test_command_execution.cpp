/*
 * Command execution (FFBoardMainCommandThread.cpp, CommandHandler.cpp, CommandInterface.cpp)
 *
 * Tests how parsed commands are executed and how results are distributed to the
 * command interfaces. Independent of any transport: commands are injected and
 * results are observed with MockCommandInterface.
 */
#include "doctest.h"
#include "TestHelpers.h"
#include "FirmwareFixture.h"
#include "MockCommandHandler.h"
#include "MockCommandInterface.h"

namespace {

struct ExecutionFixture : CommandSystemFixture {
	MockCommandHandler mock{"mock", CLSID_CUSTOM, 0};
	MockCommandInterface itf;

	/** Executes one command and returns all results received by the interface */
	std::vector<CommandResult> execute(MockCmd cmd, CMDtype type, int64_t val = 0, int64_t adr = 0) {
		itf.replies.clear();
		itf.queueCommand(mock, MockCommandHandler::id(cmd), type, val, adr);
		process();
		return itf.allResults();
	}

	std::vector<CommandResult> executeInternal(CommandHandlerCommands cmd, CMDtype type, int64_t adr = 0) {
		itf.replies.clear();
		itf.queueCommand(mock, static_cast<uint32_t>(cmd), type, 0, adr);
		process();
		return itf.allResults();
	}
};

} // namespace

TEST_SUITE("CommandExecution") {

// ---------------------------------------------------------------------------
// Execution and results
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ExecutionFixture, "nothing is executed before the command thread runs") {
	itf.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::get);
	CHECK(mock.received.empty());
	CHECK(itf.hasNewCommands());
	process();
	CHECK(mock.received.size() == 1);
	CHECK_FALSE(itf.hasNewCommands());
}

TEST_CASE_FIXTURE(ExecutionFixture, "get command returns the value") {
	mock.value = 1234;
	auto results = execute(MockCmd::value, CMDtype::get);

	REQUIRE(results.size() == 1);
	CHECK(results[0].type == CommandStatus::OK);
	CHECK(results[0].commandHandler == &mock);
	CHECK(results[0].handlerId == mock.getCommandHandlerID());
	CHECK(results[0].originalCommand.cmdId == MockCommandHandler::id(MockCmd::value));
	CHECK(results[0].originalCommand.type == CMDtype::get);
	CHECK(results[0].originalCommand.originalInterface == &itf);
	REQUIRE(results[0].reply.size() == 1);
	CHECK(results[0].reply[0].type == CommandReplyType::INT);
	CHECK(results[0].reply[0].val == 1234);
}

TEST_CASE_FIXTURE(ExecutionFixture, "set command changes the value and returns an empty OK result") {
	auto results = execute(MockCmd::value, CMDtype::set, -55);

	CHECK(mock.value == -55);
	REQUIRE(results.size() == 1);
	CHECK(results[0].type == CommandStatus::OK);
	CHECK(results[0].reply.empty());
	CHECK(results[0].originalCommand.val == -55);
}

TEST_CASE_FIXTURE(ExecutionFixture, "address commands pass value and address") {
	execute(MockCmd::addr, CMDtype::setat, 77, 5);
	CHECK(mock.values[5] == 77);

	auto results = execute(MockCmd::addr, CMDtype::getat, 0, 5);
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].reply.size() == 1);
	CHECK(results[0].reply[0].type == CommandReplyType::DOUBLEINTS);
	CHECK(results[0].reply[0].val == 77);
	CHECK(results[0].reply[0].adr == 5);
}

TEST_CASE_FIXTURE(ExecutionFixture, "info command returns a string") {
	auto results = execute(MockCmd::info, CMDtype::info);
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].reply.size() == 1);
	CHECK(results[0].reply[0].type == CommandReplyType::STRING);
	CHECK(results[0].reply[0].reply == "info text");
}

TEST_CASE_FIXTURE(ExecutionFixture, "handler error is passed to the interface") {
	auto results = execute(MockCmd::fail, CMDtype::get);
	REQUIRE(results.size() == 1);
	CHECK(results[0].type == CommandStatus::ERR);
	CHECK(results[0].reply.empty());
}

TEST_CASE_FIXTURE(ExecutionFixture, "NO_REPLY status sends nothing") {
	auto results = execute(MockCmd::noreply, CMDtype::get);
	CHECK(mock.received.size() == 1);
	CHECK(results.empty());
}

TEST_CASE_FIXTURE(ExecutionFixture, "multiple replies of one command are kept in order") {
	auto results = execute(MockCmd::multi, CMDtype::get);
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].reply.size() == 3);
	CHECK(results[0].reply[0].val == 1);
	CHECK(results[0].reply[1].val == 2);
	CHECK(results[0].reply[2].val == 3);
}

TEST_CASE_FIXTURE(ExecutionFixture, "commands of a batch are executed in order with separate results") {
	itf.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::set, 1);
	itf.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::get);
	itf.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::set, 2);
	itf.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::get);
	process();

	REQUIRE(itf.replies.size() == 4); // One sendReplies call per command
	CHECK(itf.replies[1].results[0].reply[0].val == 1);
	CHECK(itf.replies[3].results[0].reply[0].val == 2);
	CHECK(itf.batches == 1);
}

// ---------------------------------------------------------------------------
// Access flags
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ExecutionFixture, "command type must be allowed by the command flags") {
	// Characterization: not allowed access types are dropped without any reply
	mock.value = 5;

	SUBCASE("write to read only command") {
		auto results = execute(MockCmd::readonly, CMDtype::set, 9);
		CHECK(mock.value == 5);
		CHECK(mock.received.empty());
		CHECK(results.empty());
	}
	SUBCASE("read from write only command") {
		auto results = execute(MockCmd::writeonly, CMDtype::get);
		CHECK(mock.received.empty());
		CHECK(results.empty());
	}
	SUBCASE("address access to command without address flags") {
		CHECK(execute(MockCmd::value, CMDtype::getat, 0, 1).empty());
		CHECK(execute(MockCmd::value, CMDtype::setat, 1, 1).empty());
		CHECK(mock.received.empty());
	}
	SUBCASE("plain access to address only command") {
		CHECK(execute(MockCmd::addr, CMDtype::get).empty());
		CHECK(execute(MockCmd::addr, CMDtype::set, 1).empty());
		CHECK(mock.received.empty());
	}
	SUBCASE("info on command without info flag") {
		CHECK(execute(MockCmd::value, CMDtype::info).empty());
		CHECK(mock.received.empty());
	}
	SUBCASE("error type from parser") {
		CHECK(execute(MockCmd::value, CMDtype::err).empty());
		CHECK(mock.received.empty());
	}
}

TEST_CASE_FIXTURE(ExecutionFixture, "allowed access types are executed") {
	CHECK(execute(MockCmd::readonly, CMDtype::get).size() == 1);
	CHECK(execute(MockCmd::writeonly, CMDtype::set, 3).size() == 1);
	CHECK(mock.value == 3);
}

TEST_CASE_FIXTURE(ExecutionFixture, "command without access flags accepts every type") {
	CHECK(execute(MockCmd::untyped, CMDtype::set, 4).size() == 1);
	CHECK(execute(MockCmd::untyped, CMDtype::get).size() == 1);
	CHECK(execute(MockCmd::untyped, CMDtype::getat, 0, 1).size() == 1);
	CHECK(mock.received.size() == 3);
}

TEST_CASE_FIXTURE(ExecutionFixture, "command with an unknown id is rejected" * doctest::skip() *
		doctest::description("KNOWN ISSUE (crash): executeCommands dereferences the command definition before checking it for nullptr. Skipped because it terminates the test run")) {
	auto results = execute(MockCmd::unknown, CMDtype::get);
	CHECK(mock.received.empty());
	CHECK(results.empty());
}

// ---------------------------------------------------------------------------
// Internal commands available for every handler
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ExecutionFixture, "internal identification commands") {
	SUBCASE("id") {
		auto results = executeInternal(CommandHandlerCommands::id, CMDtype::get);
		REQUIRE(results.size() == 1);
		REQUIRE(results[0].reply.size() == 1);
		CHECK(results[0].reply[0].val == CLSID_CUSTOM);
	}
	SUBCASE("name") {
		auto results = executeInternal(CommandHandlerCommands::name, CMDtype::get);
		REQUIRE(results.size() == 1);
		REQUIRE(results[0].reply.size() == 1);
		CHECK(results[0].reply[0].reply == "Mock handler");
	}
	SUBCASE("instance") {
		MockCommandHandler second{"mock", CLSID_CUSTOM, 3};
		itf.queueCommand(second, (uint32_t)CommandHandlerCommands::instance, CMDtype::get);
		process();
		auto results = itf.allResults();
		REQUIRE(results.size() == 1);
		CHECK(results[0].reply[0].val == 3);
	}
	SUBCASE("cmduid") {
		auto results = executeInternal(CommandHandlerCommands::cmdhandleruid, CMDtype::get);
		REQUIRE(results.size() == 1);
		CHECK(results[0].reply[0].val == mock.getCommandHandlerID());
	}
	CHECK(mock.received.empty()); // Never passed to the handlers command function
}

TEST_CASE_FIXTURE(ExecutionFixture, "cmdinfo returns the flags of a command") {
	auto results = executeInternal(CommandHandlerCommands::cmdinfo, CMDtype::getat, MockCommandHandler::id(MockCmd::addr));
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].reply.size() == 1);
	CHECK(results[0].reply[0].val == (CMDFLAG_GETADR | CMDFLAG_SETADR));

	results = executeInternal(CommandHandlerCommands::cmdinfo, CMDtype::getat, MockCommandHandler::id(MockCmd::unknown));
	REQUIRE(results.size() == 1);
	CHECK(results[0].reply[0].val == -1);
}

TEST_CASE_FIXTURE(ExecutionFixture, "help lists all commands") {
	auto results = executeInternal(CommandHandlerCommands::help, CMDtype::get);
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].reply.size() == 1);
	const std::string &help = results[0].reply[0].reply;
	CHECK(help.find("Mock handler(mock.0)") != std::string::npos);
	CHECK(help.find("Mock command handler") != std::string::npos);
	for (const char *cmd : {"value", "addr", "readonly", "writeonly", "info", "stronly", "id", "help"}) {
		CAPTURE(std::string(cmd));
		CHECK(help.find(std::string("\n") + cmd + "\t") != std::string::npos);
	}
	CHECK(help.find("addr\t( WA RA )") != std::string::npos);
	CHECK(help.find("value\t( R W )") != std::string::npos);
}

TEST_CASE_FIXTURE(ExecutionFixture, "help info returns a csv list") {
	auto results = executeInternal(CommandHandlerCommands::help, CMDtype::info);
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].reply.size() == 1);
	const std::string &help = results[0].reply[0].reply;
	CHECK(help.find("mock.0,0x539,Mock handler") != std::string::npos);
	CHECK(help.find("addr,0x1,Value at address, WA RA") != std::string::npos);
}

TEST_CASE_FIXTURE(ExecutionFixture, "disabled handlers only answer internal commands") {
	mock.setCommandsEnabled(false);

	auto results = execute(MockCmd::value, CMDtype::get);
	CHECK(mock.received.empty());
	REQUIRE(results.size() == 1);
	CHECK(results[0].type == CommandStatus::NOT_FOUND);

	results = executeInternal(CommandHandlerCommands::id, CMDtype::get);
	REQUIRE(results.size() == 1);
	CHECK(results[0].type == CommandStatus::OK);
}

// ---------------------------------------------------------------------------
// Distribution of results
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ExecutionFixture, "results are passed to all interfaces with the origin interface") {
	MockCommandInterface observer;
	execute(MockCmd::value, CMDtype::set, 8);

	REQUIRE(itf.replies.size() == 1);
	CHECK(itf.replies[0].originalInterface == &itf);
	REQUIRE(observer.replies.size() == 1);
	CHECK(observer.replies[0].originalInterface == &itf);
	CHECK(observer.replies[0].results[0].originalCommand.val == 8);
}

TEST_CASE_FIXTURE(ExecutionFixture, "batch done is signalled to all interfaces once per batch") {
	MockCommandInterface observer;
	itf.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::get);
	itf.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::get);
	process();
	CHECK(itf.batches == 1);
	CHECK(observer.batches == 1);
}

TEST_CASE_FIXTURE(ExecutionFixture, "reply for a busy interface is dropped after 100ms without blocking others") {
	MockCommandInterface busy;
	busy.ready = false;
	uint32_t start = HAL_GetTick();

	auto results = execute(MockCmd::value, CMDtype::get);

	CHECK(results.size() == 1);
	CHECK(busy.replies.empty());
	CHECK(busy.batches == 1);
	uint32_t elapsed = HAL_GetTick() - start;
	CHECK(elapsed >= 99);
	CHECK(elapsed <= 100);
}

TEST_CASE_FIXTURE(ExecutionFixture, "handler broadcasts are sent to all interfaces") {
	MockCommandInterface observer;
	mock.broadcastCommandReply(CommandReply((int64_t)42), MockCommandHandler::id(MockCmd::value), CMDtype::get);

	for (MockCommandInterface *i : {&itf, &observer}) {
		REQUIRE(i->replies.size() == 1);
		CHECK(i->replies[0].originalInterface == nullptr);
		REQUIRE(i->replies[0].results.size() == 1);
		const CommandResult &result = i->replies[0].results[0];
		CHECK(result.type == CommandStatus::BROADCAST);
		CHECK(result.commandHandler == &mock);
		CHECK(result.originalCommand.cmdId == MockCommandHandler::id(MockCmd::value));
		CHECK(result.originalCommand.type == CMDtype::get);
		CHECK(result.originalCommand.instance == 0);
		REQUIRE(result.reply.size() == 1);
		CHECK(result.reply[0].val == 42);
	}
}

TEST_CASE_FIXTURE(ExecutionFixture, "async replies can target a single interface") {
	MockCommandInterface observer;
	mock.sendCommandReplyAsync(CommandReply((int64_t)42), MockCommandHandler::id(MockCmd::value), CMDtype::get, &observer);

	CHECK(itf.replies.empty());
	REQUIRE(observer.replies.size() == 1);
	CHECK(observer.replies[0].originalInterface == &observer);
	CHECK(observer.replies[0].results[0].type == CommandStatus::BROADCAST);
}

TEST_CASE_FIXTURE(ExecutionFixture, "async reply without interface is a broadcast") {
	MockCommandInterface observer;
	mock.sendCommandReplyAsync(CommandReply((int64_t)42), MockCommandHandler::id(MockCmd::value), CMDtype::get, nullptr);
	CHECK(itf.replies.size() == 1);
	CHECK(observer.replies.size() == 1);
}

TEST_CASE_FIXTURE(ExecutionFixture, "interfaces unregister when destroyed") {
	size_t before = CommandInterface::cmdInterfaces.size();
	{
		MockCommandInterface temporary;
		CHECK(CommandInterface::cmdInterfaces.size() == before + 1);
	}
	CHECK(CommandInterface::cmdInterfaces.size() == before);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// Command handler registry (CommandHandler.cpp)
// ---------------------------------------------------------------------------
TEST_SUITE("CommandHandler") {

TEST_CASE_FIXTURE(FirmwareFixture, "handlers register and unregister in the global list") {
	size_t before = registeredCommandHandlers();
	{
		MockCommandHandler a{"a", 0x100, 0};
		CHECK(registeredCommandHandlers() == before + 1);
		CHECK(CommandHandler::isInHandlerList(&a));
	}
	CHECK(registeredCommandHandlers() == before);
}

TEST_CASE_FIXTURE(FirmwareFixture, "handlers get unique handler ids which are reused") {
	MockCommandHandler a{"a", 0x100, 0};
	uint16_t freedId;
	{
		MockCommandHandler b{"b", 0x101, 0};
		CHECK(a.getCommandHandlerID() != b.getCommandHandlerID());
		CHECK(CommandHandler::getHandlerFromHandlerId(b.getCommandHandlerID()) == &b);
		freedId = b.getCommandHandlerID();
	}
	CHECK(CommandHandler::getHandlerFromHandlerId(freedId) == nullptr);
	MockCommandHandler c{"c", 0x102, 0};
	CHECK(c.getCommandHandlerID() == freedId);
	CHECK(a.getCommandHandlerID() != 0);
}

TEST_CASE_FIXTURE(FirmwareFixture, "lookup by class name and instance") {
	MockCommandHandler a0{"a", 0x100, 0};
	MockCommandHandler a1{"a", 0x100, 1};
	MockCommandHandler b0{"b", 0x101, 0};

	CHECK(CommandHandler::getHandlerFromClassName("a", 0) == &a0);
	CHECK(CommandHandler::getHandlerFromClassName("a", 1) == &a1);
	CHECK(CommandHandler::getHandlerFromClassName("a", 2) == nullptr);
	CHECK(CommandHandler::getHandlerFromClassName("a") == &a0); // 0xFF returns the first
	CHECK(CommandHandler::getHandlerFromClassName("b", 0) == &b0);
	CHECK(CommandHandler::getHandlerFromClassName("c") == nullptr);

	auto all = CommandHandler::getHandlersFromClassName("a");
	REQUIRE(all.size() == 2);
	CHECK(all[0] == &a0);
	CHECK(all[1] == &a1);
	CHECK(CommandHandler::getHandlersFromClassName("c").empty());
}

TEST_CASE_FIXTURE(FirmwareFixture, "lookup by class id and instance") {
	MockCommandHandler a0{"a", 0x100, 0};
	MockCommandHandler a1{"a", 0x100, 1};

	CHECK(CommandHandler::getHandlerFromId(0x100, 0) == &a0);
	CHECK(CommandHandler::getHandlerFromId(0x100, 1) == &a1);
	CHECK(CommandHandler::getHandlerFromId(0x100, 2) == nullptr);
	CHECK(CommandHandler::getHandlerFromId(0x100) == &a0);
	CHECK(CommandHandler::getHandlerFromId(0x999) == nullptr);
	CHECK(CommandHandler::getHandlersFromId(0x100).size() == 2);
	CHECK(CommandHandler::getHandlersFromId(0x999).empty());
}

TEST_CASE_FIXTURE(FirmwareFixture, "class name and id can be translated") {
	MockCommandHandler a{"a", 0x100, 0};
	CHECK(CommandHandler::getClassIdFromName("a") == 0x100);
	CHECK(std::string(CommandHandler::getClassNameFromId(0x100)) == "a");
	CHECK(CommandHandler::getClassNameFromId(0x999) == nullptr);
}

TEST_CASE_FIXTURE(FirmwareFixture, "command lookup by name and id") {
	MockCommandHandler mock;
	CmdHandlerCommanddef *byName = mock.getCommandFromName("addr");
	REQUIRE(byName != nullptr);
	CHECK(byName->cmdId == MockCommandHandler::id(MockCmd::addr));
	CHECK(byName->flags == (CMDFLAG_GETADR | CMDFLAG_SETADR));

	CmdHandlerCommanddef *byId = mock.getCommandFromId(MockCommandHandler::id(MockCmd::addr));
	CHECK(byId == byName);

	CHECK(mock.getCommandFromName("nothere") == nullptr);
	CHECK(mock.getCommandFromId(MockCommandHandler::id(MockCmd::unknown)) == nullptr);
}

TEST_CASE_FIXTURE(FirmwareFixture, "command lookup honors ignored flags and debug mode") {
	MockCommandHandler mock;
	CHECK(mock.getCommandFromName("hidonly") != nullptr);
	CHECK(mock.getCommandFromName("hidonly", CMDFLAG_HID_ONLY) == nullptr);
	CHECK(mock.getCommandFromId(MockCommandHandler::id(MockCmd::stronly), CMDFLAG_STR_ONLY) == nullptr);

	CHECK(mock.getCommandFromName("debug") == nullptr);
	CHECK_FALSE(mock.isValidCommandId(MockCommandHandler::id(MockCmd::debug)));
	SystemCommands::debugMode = true;
	CHECK(mock.getCommandFromName("debug") != nullptr);
	CHECK(mock.isValidCommandId(MockCommandHandler::id(MockCmd::debug)));
}

TEST_CASE_FIXTURE(FirmwareFixture, "command id validation with ignored and required flags") {
	MockCommandHandler mock;
	uint32_t value = MockCommandHandler::id(MockCmd::value);
	CHECK(mock.isValidCommandId(value));
	CHECK(mock.isValidCommandId(value, 0, CMDFLAG_GET));
	CHECK(mock.isValidCommandId(value, 0, CMDFLAG_GET | CMDFLAG_SET));
	CHECK_FALSE(mock.isValidCommandId(value, 0, CMDFLAG_GETADR));
	CHECK_FALSE(mock.isValidCommandId(value, CMDFLAG_SET));
	CHECK_FALSE(mock.isValidCommandId(MockCommandHandler::id(MockCmd::unknown)));
}

TEST_CASE_FIXTURE(FirmwareFixture, "handleGetSet helper reads and writes a variable") {
	std::vector<CommandReply> replies;
	uint8_t variable = 7;
	ParsedCommand cmd;

	cmd.type = CMDtype::get;
	CHECK(CommandHandler::handleGetSet(cmd, replies, variable) == CommandStatus::OK);
	REQUIRE(replies.size() == 1);
	CHECK(replies[0].val == 7);

	cmd.type = CMDtype::set;
	cmd.val = 300; // Truncated to the variable type
	CHECK(CommandHandler::handleGetSet(cmd, replies, variable) == CommandStatus::OK);
	CHECK(variable == (uint8_t)300);

	cmd.type = CMDtype::info;
	CHECK(CommandHandler::handleGetSet(cmd, replies, variable) == CommandStatus::ERR);
}

} // TEST_SUITE
