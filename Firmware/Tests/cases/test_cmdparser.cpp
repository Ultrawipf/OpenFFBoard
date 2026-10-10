/*
 * String command parser (CmdParser.cpp)
 *
 * Syntax under test (see StringCommandInterface::getHelpstring):
 *   Get:  cls.(instance.)cmd?      or cls.(instance.)cmd?adr
 *   Set:  cls.(instance.)cmd=val   or cls.(instance.)cmd=val?adr
 *   Info: cls.(instance.)cmd!
 * Commands are terminated by ; newline, carriage return or space.
 */
#include "doctest.h"
#include "TestHelpers.h"
#include "FirmwareFixture.h"
#include "MockCommandHandler.h"

#include "CmdParser.h"

namespace {

/** Adds text to the parser. The parser modifies the buffer so a copy is passed */
bool feed(CmdParser &parser, std::string text) {
	uint32_t len = text.size();
	return parser.add(text.data(), &len);
}

struct ParserFixture : FirmwareFixture {
	MockCommandHandler mock{"mock", CLSID_CUSTOM, 0};
	CmdParser parser{512};
	std::vector<ParsedCommand> commands;

	/** Feeds and parses a complete string. Returns the result of parse() */
	bool parse(const std::string &text) {
		feed(parser, text);
		return parser.parse(commands);
	}
};

} // namespace

TEST_SUITE("CmdParser") {

// ---------------------------------------------------------------------------
// Command types
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ParserFixture, "get command") {
	CHECK(parse("mock.value?;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].type == CMDtype::get);
	CHECK(commands[0].target == &mock);
	CHECK(commands[0].cmdId == MockCommandHandler::id(MockCmd::value));
	CHECK(commands[0].instance == 0xFF); // No instance given
}

TEST_CASE_FIXTURE(ParserFixture, "command without type suffix is a get command") {
	CHECK(parse("mock.value;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].type == CMDtype::get);
	CHECK(commands[0].cmdId == MockCommandHandler::id(MockCmd::value));
}

TEST_CASE_FIXTURE(ParserFixture, "set command with decimal values") {
	SUBCASE("positive") {
		CHECK(parse("mock.value=42;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].type == CMDtype::set);
		CHECK(commands[0].val == 42);
	}
	SUBCASE("negative") {
		CHECK(parse("mock.value=-42;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].type == CMDtype::set);
		CHECK(commands[0].val == -42);
	}
	SUBCASE("explicit positive sign") {
		CHECK(parse("mock.value=+7;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].val == 7);
	}
	SUBCASE("zero") {
		CHECK(parse("mock.value=0;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].type == CMDtype::set);
		CHECK(commands[0].val == 0);
	}
	SUBCASE("64 bit range") {
		CHECK(parse("mock.value=9223372036854775807;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].val == INT64_MAX);
	}
	SUBCASE("values above 32 bit") {
		CHECK(parse("mock.value=4294967296;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].val == 4294967296LL);
	}
}

TEST_CASE_FIXTURE(ParserFixture, "set command with hex value") {
	CHECK(parse("mock.value=x1F;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].type == CMDtype::set);
	CHECK(commands[0].val == 0x1F);
}

TEST_CASE_FIXTURE(ParserFixture, "get with address") {
	SUBCASE("decimal") {
		CHECK(parse("mock.addr?5;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].type == CMDtype::getat);
		CHECK(commands[0].adr == 5);
		CHECK(commands[0].cmdId == MockCommandHandler::id(MockCmd::addr));
	}
	SUBCASE("hex") {
		CHECK(parse("mock.addr?xA0;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].type == CMDtype::getat);
		CHECK(commands[0].adr == 0xA0);
	}
}

TEST_CASE_FIXTURE(ParserFixture, "set with address") {
	SUBCASE("decimal") {
		CHECK(parse("mock.addr=7?3;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].type == CMDtype::setat);
		CHECK(commands[0].val == 7);
		CHECK(commands[0].adr == 3);
	}
	SUBCASE("negative value") {
		CHECK(parse("mock.addr=-70?3;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].type == CMDtype::setat);
		CHECK(commands[0].val == -70);
		CHECK(commands[0].adr == 3);
	}
	SUBCASE("hex value and address") {
		CHECK(parse("mock.addr=xFF?x10;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].type == CMDtype::setat);
		CHECK(commands[0].val == 0xFF);
		CHECK(commands[0].adr == 0x10);
	}
	SUBCASE("multi digit value and address") {
		CHECK(parse("mock.addr=12345?678;"));
		REQUIRE(commands.size() == 1);
		CHECK(commands[0].val == 12345);
		CHECK(commands[0].adr == 678);
	}
}

TEST_CASE_FIXTURE(ParserFixture, "info command") {
	CHECK(parse("mock.info!;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].type == CMDtype::info);
	CHECK(commands[0].cmdId == MockCommandHandler::id(MockCmd::info));
}

TEST_CASE_FIXTURE(ParserFixture, "set without value creates no command") {
	CHECK_FALSE(parse("mock.value=;"));
	CHECK(commands.empty());
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).size() == 1);
}

// ---------------------------------------------------------------------------
// Addressing of classes and instances
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ParserFixture, "instance number selects one handler") {
	MockCommandHandler second{"mock", CLSID_CUSTOM, 1};

	CHECK(parse("mock.1.value?;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].target == &second);
	CHECK(commands[0].instance == 1);

	commands.clear();
	CHECK(parse("mock.0.value=3;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].target == &mock);
	CHECK(commands[0].instance == 0);
	CHECK(commands[0].val == 3);
}

TEST_CASE_FIXTURE(ParserFixture, "command without instance targets all instances of a class") {
	MockCommandHandler second{"mock", CLSID_CUSTOM, 1};
	MockCommandHandler other{"other", CLSID_CUSTOM + 1, 0};

	CHECK(parse("mock.value?;"));
	REQUIRE(commands.size() == 2);
	CHECK(commands[0].target == &mock);
	CHECK(commands[0].instance == 0);
	CHECK(commands[1].target == &second);
	CHECK(commands[1].instance == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "not existing instance is not found") {
	CHECK_FALSE(parse("mock.3.value?;"));
	CHECK(commands.empty());
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).size() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "class name selects the handler") {
	MockCommandHandler other{"other", CLSID_CUSTOM + 1, 0};
	CHECK(parse("other.value?;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].target == &other);
}

TEST_CASE_FIXTURE(ParserFixture, "command without class name is sent to the sys class") {
	MockCommandHandler sys{CMDCLSTR_SYS, CMDCLSID_SYS, 0};
	CHECK(parse("value?;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].target == &sys);
	CHECK(commands[0].type == CMDtype::get);
}

TEST_CASE_FIXTURE(ParserFixture, "instance numbers above 9 can be addressed" * KNOWN_ISSUE("Only the first character after the class name is used as instance. mock.12.cmd addresses instance 1")) {
	MockCommandHandler one{"mock", CLSID_CUSTOM, 1};
	MockCommandHandler twelve{"mock", CLSID_CUSTOM, 12};

	parse("mock.12.value?;");
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].target == &twelve);
}

// ---------------------------------------------------------------------------
// Command lookup
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ParserFixture, "unknown command creates an error and no command") {
	CHECK_FALSE(parse("mock.doesnotexist?;"));
	CHECK(commands.empty());
	std::vector<std::string> errors = errorsWithCode(ErrorCode::cmdNotFound);
	REQUIRE(errors.size() == 1);
	CHECK(errors[0].find("mock.doesnotexist") != std::string::npos);
}

TEST_CASE_FIXTURE(ParserFixture, "unknown class creates an error and no command") {
	CHECK_FALSE(parse("nothere.value?;"));
	CHECK(commands.empty());
	std::vector<std::string> errors = errorsWithCode(ErrorCode::cmdNotFound);
	REQUIRE(errors.size() == 1);
	CHECK(errors[0].find("nothere.value") != std::string::npos);
}

TEST_CASE_FIXTURE(ParserFixture, "internal commands of every handler are available") {
	CHECK(parse("mock.id?;mock.name?;mock.help?;mock.cmduid?;mock.instance?;mock.cmdinfo?0;"));
	REQUIRE(commands.size() == 6);
	CHECK(commands[0].cmdId == (uint32_t)CommandHandlerCommands::id);
	CHECK(commands[1].cmdId == (uint32_t)CommandHandlerCommands::name);
	CHECK(commands[2].cmdId == (uint32_t)CommandHandlerCommands::help);
	CHECK(commands[3].cmdId == (uint32_t)CommandHandlerCommands::cmdhandleruid);
	CHECK(commands[4].cmdId == (uint32_t)CommandHandlerCommands::instance);
	CHECK(commands[5].cmdId == (uint32_t)CommandHandlerCommands::cmdinfo);
	CHECK(commands[5].type == CMDtype::getat);
}

TEST_CASE_FIXTURE(ParserFixture, "HID only commands are hidden from the string parser") {
	CHECK_FALSE(parse("mock.hidonly?;"));
	CHECK(commands.empty());
}

TEST_CASE_FIXTURE(ParserFixture, "string only commands are available") {
	CHECK(parse("mock.stronly?;"));
	CHECK(commands.size() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "debug commands are only found in debug mode") {
	CHECK_FALSE(parse("mock.debug?;"));
	CHECK(commands.empty());

	SystemCommands::debugMode = true;
	CHECK(parse("mock.debug?;"));
	CHECK(commands.size() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "command names are case sensitive") {
	CHECK_FALSE(parse("mock.VALUE?;"));
	CHECK(commands.empty());
}

// ---------------------------------------------------------------------------
// Buffering and termination
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ParserFixture, "all terminators end a command") {
	for (const char *terminator : {";", "\n", "\r", " ", "\r\n", ";\n"}) {
		CAPTURE(std::string(terminator).size());
		commands.clear();
		CHECK(feed(parser, std::string("mock.value=1") + terminator));
		CHECK(parser.parse(commands));
		CHECK(commands.size() == 1);
	}
}

TEST_CASE_FIXTURE(ParserFixture, "add reports if a terminator was received") {
	CHECK_FALSE(feed(parser, "mock.value?"));
	CHECK(feed(parser, ";"));
}

TEST_CASE_FIXTURE(ParserFixture, "multiple commands in one buffer are parsed in order") {
	CHECK(parse("mock.value=1;mock.addr?2\nmock.info! mock.value?;"));
	REQUIRE(commands.size() == 4);
	CHECK(commands[0].type == CMDtype::set);
	CHECK(commands[0].val == 1);
	CHECK(commands[1].type == CMDtype::getat);
	CHECK(commands[1].adr == 2);
	CHECK(commands[2].type == CMDtype::info);
	CHECK(commands[3].type == CMDtype::get);
}

TEST_CASE_FIXTURE(ParserFixture, "commands can arrive in fragments") {
	CHECK_FALSE(feed(parser, "mock.va"));
	CHECK_FALSE(parser.parse(commands));
	CHECK(commands.empty());

	CHECK_FALSE(feed(parser, "lue=1"));
	CHECK(feed(parser, "5;mock."));
	CHECK(parser.parse(commands));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].val == 15);

	// The incomplete rest stays in the buffer
	commands.clear();
	CHECK(feed(parser, "value?;"));
	CHECK(parser.parse(commands));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].type == CMDtype::get);
}

TEST_CASE_FIXTURE(ParserFixture, "byte wise input like from a uart") {
	std::string text = "mock.addr=12?34;";
	bool terminated = false;
	for (char c : text) {
		terminated = feed(parser, std::string(1, c));
	}
	CHECK(terminated);
	CHECK(parser.parse(commands));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].type == CMDtype::setat);
	CHECK(commands[0].val == 12);
	CHECK(commands[0].adr == 34);
}

TEST_CASE_FIXTURE(ParserFixture, "parsed commands are removed from the buffer") {
	CHECK(parse("mock.value?;"));
	commands.clear();
	CHECK_FALSE(parser.parse(commands));
	CHECK(commands.empty());
}

TEST_CASE_FIXTURE(ParserFixture, "empty commands are ignored") {
	CHECK_FALSE(parse(";;;\n\r  ;"));
	CHECK(commands.empty());
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).empty());
}

TEST_CASE_FIXTURE(ParserFixture, "clear discards buffered data") {
	feed(parser, "mock.val");
	parser.clear();
	CHECK(parse("mock.value?;"));
	CHECK(commands.size() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "stale fragments are discarded after the clear timeout") {
	parser.setClearBufferTimeout(500);
	feed(parser, "mock.garbage");
	HostPlatform::advanceMs(501);

	CHECK(parse("mock.value?;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].cmdId == MockCommandHandler::id(MockCmd::value));
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).empty());
}

TEST_CASE_FIXTURE(ParserFixture, "fragments are kept within the clear timeout") {
	parser.setClearBufferTimeout(500);
	feed(parser, "mock.va");
	HostPlatform::advanceMs(499);

	CHECK(parse("lue?;"));
	CHECK(commands.size() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "fragments are kept forever without clear timeout") {
	feed(parser, "mock.va");
	HostPlatform::advanceMs(60000);
	CHECK(parse("lue?;"));
	CHECK(commands.size() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "parser recovers after the buffer overflows") {
	// Fill the buffer with data that never terminates
	feed(parser, std::string(300, 'a'));
	feed(parser, std::string(205, 'b'));
	CHECK(parser.bufferCapacity() < 13);

	// A terminated chunk that does not fit resets the buffer and is lost
	CHECK_FALSE(feed(parser, "mock.value=1;"));
	CHECK_FALSE(parser.parse(commands));
	CHECK(commands.empty());

	// Next command works again
	CHECK(parse("mock.value=2;"));
	REQUIRE(commands.size() == 1);
	CHECK(commands[0].val == 2);
}

TEST_CASE_FIXTURE(ParserFixture, "buffer capacity is limited") {
	CmdParser large(100000);
	CHECK(large.bufferCapacity() <= CMDPARSER_MAX_VALID_CAPACITY);
}

TEST_CASE_FIXTURE(ParserFixture, "free buffer capacity decreases when data is buffered") {
	int32_t empty = parser.bufferCapacity();
	CHECK(empty > 0);
	CHECK(empty <= 512);
	feed(parser, "0123456789");
	CHECK(parser.bufferCapacity() == empty - 10);
}

TEST_CASE_FIXTURE(ParserFixture, "free buffer capacity is correct at any time" * KNOWN_ISSUE("CmdParser::bufferCapacity compares the last add timestamp with the timeout duration and reports an empty buffer once the tick is above the timeout")) {
	HostPlatform::setTimeMs(10000);
	int32_t empty = parser.bufferCapacity();
	feed(parser, "0123456789");
	CHECK(parser.bufferCapacity() == empty - 10);
}

// ---------------------------------------------------------------------------
// Malformed input
// ---------------------------------------------------------------------------

TEST_CASE_FIXTURE(ParserFixture, "invalid command after a valid command is reported" * KNOWN_ISSUE("The found flag is shared by all commands of a buffer. Invalid commands following a valid one are dropped silently")) {
	parse("mock.value?;mock.doesnotexist?;");
	CHECK(commands.size() == 1);
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).size() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "invalid command before a valid command is reported") {
	CHECK(parse("mock.doesnotexist?;mock.value?;"));
	CHECK(commands.size() == 1);
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).size() == 1);
}

TEST_CASE("non numeric values are rejected without blocking the parser" * KNOWN_ISSUE("CmdParser::parse never returns for cmd=text or cmd?text. The continue statement does not advance the position")) {
	FirmwareFixture::resetGlobals();
	// The parser runs in a separate thread which owns all state because it keeps running if the parser hangs.
	// A unique class name is used because the handler stays registered in that case.
	struct State {
		MockCommandHandler mock{"hangtest", CLSID_CUSTOM + 0x100, 0};
		CmdParser parser{512};
		std::vector<ParsedCommand> commands;
	};
	static auto *abandoned = new std::vector<std::shared_ptr<State>>(); // Never freed on purpose

	for (const char *text : {"hangtest.value=abc;", "hangtest.addr?abc;", "hangtest.value=-;"}) {
		CAPTURE(std::string(text));
		auto state = std::make_shared<State>();
		feed(state->parser, text);

		bool finished = finishesWithin(std::chrono::milliseconds(300), [state]() { state->parser.parse(state->commands); });
		if (!finished) {
			abandoned->push_back(state);
		}
		REQUIRE(finished);
		CHECK(state->commands.empty());
	}
}

} // TEST_SUITE
