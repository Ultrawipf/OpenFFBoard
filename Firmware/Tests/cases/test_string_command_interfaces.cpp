/*
 * String based command interfaces (CommandInterface.cpp)
 *
 *  - StringCommandInterface: reply formatting  [cls.inst.cmd?|reply]\n
 *  - CDC_CommandInterface:   USB serial port. Replies are recorded by the fake TinyUSB (HostUsb)
 *  - UART_CommandInterface:  external uart. Bytes are injected/recorded by the fake HAL (HostPlatform)
 *
 * The CDC and UART tests are end to end: text in -> parser -> command thread -> handler -> text out.
 */
#include "doctest.h"
#include "TestHelpers.h"
#include "FirmwareFixture.h"
#include "MockCommandHandler.h"
#include "MockCommandInterface.h"

#include "CommandInterface.h"
#include "cpp_target_config.h"

namespace {

/** Builds a result like the command thread does */
CommandResult makeResult(MockCommandHandler &handler, MockCmd cmd, CMDtype type, CommandStatus status,
						 std::vector<CommandReply> replies = {}, int64_t val = 0, int64_t adr = 0, uint8_t instance = 0xFF) {
	CommandResult result;
	result.originalCommand.cmdId = MockCommandHandler::id(cmd);
	result.originalCommand.type = type;
	result.originalCommand.val = val;
	result.originalCommand.adr = adr;
	result.originalCommand.instance = instance;
	result.originalCommand.target = &handler;
	result.commandHandler = &handler;
	result.handlerId = handler.getCommandHandlerID();
	result.type = status;
	result.reply = std::move(replies);
	return result;
}

std::string format(const std::vector<CommandResult> &results, bool writeAsRead = false) {
	std::string out;
	StringCommandInterface::formatReply(out, results, writeAsRead);
	return out;
}

} // namespace

// ---------------------------------------------------------------------------
TEST_SUITE("StringCommandInterface") {

TEST_CASE_FIXTURE(FirmwareFixture, "reply format of the command types") {
	MockCommandHandler mock;

	SUBCASE("get with integer reply") {
		CHECK(format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::OK, {CommandReply((int64_t)5)})}) == "[mock.value?|5]\n");
	}
	SUBCASE("negative integer reply") {
		CHECK(format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::OK, {CommandReply((int64_t)-5)})}) == "[mock.value?|-5]\n");
	}
	SUBCASE("set without reply is acknowledged with OK") {
		CHECK(format({makeResult(mock, MockCmd::value, CMDtype::set, CommandStatus::OK, {}, 12)}) == "[mock.value=12|OK]\n");
	}
	SUBCASE("get with address") {
		CHECK(format({makeResult(mock, MockCmd::addr, CMDtype::getat, CommandStatus::OK, {CommandReply((int64_t)7, (int64_t)3)}, 0, 3)}) == "[mock.addr?3|7:3]\n");
	}
	SUBCASE("set with address") {
		CHECK(format({makeResult(mock, MockCmd::addr, CMDtype::setat, CommandStatus::OK, {}, 7, 3)}) == "[mock.addr=7?3|OK]\n");
	}
	SUBCASE("info") {
		CHECK(format({makeResult(mock, MockCmd::info, CMDtype::info, CommandStatus::OK, {CommandReply(std::string("some text"))})}) == "[mock.info!|some text]\n");
	}
	SUBCASE("instance number is printed if the command was addressed to an instance") {
		CHECK(format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::OK, {CommandReply((int64_t)5)}, 0, 0, 2)}) == "[mock.2.value?|5]\n");
	}
}

TEST_CASE_FIXTURE(FirmwareFixture, "reply format of the reply types") {
	MockCommandHandler mock;
	auto reply = [&](CommandReply r) { return format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::OK, {r})}); };

	CHECK(reply(CommandReply(std::string("abc"))) == "[mock.value?|abc]\n");
	CHECK(reply(CommandReply(std::string("abc"), 5)) == "[mock.value?|abc]\n");	   // STRING_OR_INT prints the string
	CHECK(reply(CommandReply(std::string("abc"), 5, 6)) == "[mock.value?|abc]\n"); // STRING_OR_DOUBLEINT prints the string
	CHECK(reply(CommandReply((int64_t)5, (int64_t)6)) == "[mock.value?|5:6]\n");
	CHECK(reply(CommandReply(CommandReplyType::ACK)) == "[mock.value?|OK]\n");
	CHECK(reply(CommandReply((int64_t)INT64_MIN)) == "[mock.value?|-9223372036854775808]\n");
}

TEST_CASE_FIXTURE(FirmwareFixture, "reply format of the status types") {
	MockCommandHandler mock;
	CHECK(format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::NOT_FOUND)}) == "[mock.value?|NOT_FOUND]\n");
	CHECK(format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::ERR)}) == "[mock.value?|ERR]\n");
	CHECK(format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::BROADCAST, {CommandReply((int64_t)1)})}) == "[mock.value?|1]\n");
	CHECK(format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::NO_REPLY)}) == "[mock.value?|None]\n");
}

TEST_CASE_FIXTURE(FirmwareFixture, "multiple replies of a command are separated by newlines") {
	MockCommandHandler mock;
	auto result = makeResult(mock, MockCmd::multi, CMDtype::get, CommandStatus::OK,
							 {CommandReply((int64_t)1), CommandReply((int64_t)2), CommandReply((int64_t)3)});
	CHECK(format({result}) == "[mock.multi?|1\n2\n3]\n");
}

TEST_CASE_FIXTURE(FirmwareFixture, "multiple results are appended") {
	MockCommandHandler mock;
	std::string out = "existing";
	StringCommandInterface::formatReply(out, {makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::OK, {CommandReply((int64_t)1)}),
											  makeResult(mock, MockCmd::value, CMDtype::set, CommandStatus::OK, {}, 2)});
	CHECK(out == "existing[mock.value?|1]\n[mock.value=2|OK]\n");
}

TEST_CASE_FIXTURE(FirmwareFixture, "writes of other interfaces are formatted as reads of the new value") {
	MockCommandHandler mock;
	CHECK(format({makeResult(mock, MockCmd::value, CMDtype::set, CommandStatus::OK, {}, 12)}, true) == "[mock.value?|12]\n");
	CHECK(format({makeResult(mock, MockCmd::addr, CMDtype::setat, CommandStatus::OK, {}, 7, 3)}, true) == "[mock.addr?3|7:3]\n");
}

TEST_CASE_FIXTURE(FirmwareFixture, "reads of other interfaces are not mirrored") {
	MockCommandHandler mock;
	CHECK(format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::OK, {CommandReply((int64_t)5)})}, true) == "");
	CHECK(format({makeResult(mock, MockCmd::info, CMDtype::info, CommandStatus::OK, {CommandReply(std::string("x"))})}, true) == "");
}

TEST_CASE_FIXTURE(FirmwareFixture, "writes following a read of another interface are still mirrored" *
		KNOWN_ISSUE("StringCommandInterface::formatReply returns at the first non write result instead of skipping it")) {
	MockCommandHandler mock;
	std::string out = format({makeResult(mock, MockCmd::value, CMDtype::get, CommandStatus::OK, {CommandReply((int64_t)5)}),
							  makeResult(mock, MockCmd::value, CMDtype::set, CommandStatus::OK, {}, 12)},
							 true);
	CHECK(out == "[mock.value?|12]\n");
}

TEST_CASE_FIXTURE(FirmwareFixture, "original command string of a result") {
	MockCommandHandler mock;
	ParsedCommand cmd;
	cmd.cmdId = MockCommandHandler::id(MockCmd::addr);
	cmd.val = 1;
	cmd.adr = 2;

	cmd.type = CMDtype::get;
	CHECK(StringCommandInterface::formatOriginalCommandFromResult(cmd, &mock) == "mock.addr?");
	cmd.type = CMDtype::getat;
	CHECK(StringCommandInterface::formatOriginalCommandFromResult(cmd, &mock) == "mock.addr?2");
	cmd.type = CMDtype::set;
	CHECK(StringCommandInterface::formatOriginalCommandFromResult(cmd, &mock) == "mock.addr=1");
	cmd.type = CMDtype::setat;
	CHECK(StringCommandInterface::formatOriginalCommandFromResult(cmd, &mock) == "mock.addr=1?2");
	cmd.type = CMDtype::info;
	CHECK(StringCommandInterface::formatOriginalCommandFromResult(cmd, &mock) == "mock.addr!");

	cmd.cmdId = MockCommandHandler::id(MockCmd::unknown);
	CHECK(StringCommandInterface::formatOriginalCommandFromResult(cmd, &mock) == "");
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
namespace {

struct CdcFixture : CommandSystemFixture {
	MockCommandHandler mock{"mock", CLSID_CUSTOM, 0};
	CDC_CommandInterface cdc;

	/** Simulates data received from the serial port (FFBoardMain::cdcRcv) */
	bool receive(std::string text) {
		uint32_t len = text.size();
		return cdc.addBuf(text.data(), &len);
	}

	/** Sends text and returns everything the device replies */
	std::string query(const std::string &text) {
		HostUsb::state().cdcTx.clear();
		receive(text);
		process();
		return HostUsb::state().cdcTx;
	}
};

} // namespace

TEST_SUITE("CDC_CommandInterface") {

TEST_CASE_FIXTURE(CdcFixture, "get command is answered on the serial port") {
	mock.value = 99;
	CHECK(query("mock.value?;") == "[mock.value?|99]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "set command changes the value and is acknowledged") {
	CHECK(query("mock.value=17;") == "[mock.value=17|OK]\n");
	CHECK(mock.value == 17);
	CHECK(query("mock.value?;") == "[mock.value?|17]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "address commands") {
	CHECK(query("mock.addr=5?2;") == "[mock.addr=5?2|OK]\n");
	CHECK(query("mock.addr?2;") == "[mock.addr?2|5:2]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "addressed instance is part of the reply") {
	MockCommandHandler second{"mock", CLSID_CUSTOM, 1};
	second.value = 3;
	CHECK(query("mock.1.value?;") == "[mock.1.value?|3]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "command without instance is answered by all instances") {
	MockCommandHandler second{"mock", CLSID_CUSTOM, 1};
	mock.value = 1;
	second.value = 2;
	CHECK(query("mock.value?;") == "[mock.0.value?|1]\n[mock.1.value?|2]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "several commands are answered in order in one transfer") {
	std::string reply = query("mock.value=1;mock.value?;mock.value=2;mock.value?;");
	CHECK(reply == "[mock.value=1|OK]\n[mock.value?|1]\n[mock.value=2|OK]\n[mock.value?|2]\n");
	CHECK(HostUsb::state().cdcWrites == 1);
}

TEST_CASE_FIXTURE(CdcFixture, "line endings of terminal programs are accepted") {
	CHECK(query("mock.value=1\r\n") == "[mock.value=1|OK]\n");
	CHECK(query("mock.value?\n") == "[mock.value?|1]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "command is executed when the terminator arrives") {
	CHECK_FALSE(receive("mock.val"));
	process();
	CHECK(mock.received.empty());
	CHECK(receive("ue=4;"));
	process();
	CHECK(mock.value == 4);
}

TEST_CASE_FIXTURE(CdcFixture, "status results") {
	CHECK(query("mock.fail?;") == "[mock.fail?|ERR]\n");
	CHECK(query("mock.ack?;") == "[mock.ack?|OK]\n");
	CHECK(query("mock.noreply?;") == "");
	CHECK(query("mock.multi?;") == "[mock.multi?|1\n2\n3]\n");
	CHECK(query("mock.str?;") == "[mock.str?|text]\n");
	CHECK(query("mock.info!;") == "[mock.info!|info text]\n");
	CHECK(query("mock.dual?;") == "[mock.dual?|0:0]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "unknown commands are not answered but create an error") {
	CHECK(query("mock.nothere?;") == "");
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).size() == 1);
}

TEST_CASE_FIXTURE(CdcFixture, "commands with a not allowed access type are not answered") {
	// Characterization: The host gets no feedback at all in this case
	CHECK(query("mock.readonly=1;") == "");
	CHECK(mock.received.empty());
}

TEST_CASE_FIXTURE(CdcFixture, "internal commands") {
	CHECK(query("mock.id?;") == "[mock.id?|1337]\n");
	CHECK(query("mock.name?;") == "[mock.name?|Mock handler]\n");
	CHECK(query("mock.instance?;") == "[mock.instance?|0]\n");
	CHECK(query("mock.cmdinfo?1;") == "[mock.cmdinfo?1|48]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "replies longer than the usb buffer are sent completely in multiple transfers") {
	HostUsb::state().cdcWriteAvailable = 64;
	std::string reply = query("mock.help?;");

	CHECK(HostUsb::state().cdcWrites > 1);
	CHECK(reply.rfind("[mock.help?|", 0) == 0);
	CHECK(reply.substr(reply.size() - 2) == "]\n");
	CHECK(reply.find("writeonly\t( W )\tWrite only value") != std::string::npos);
	CHECK(CDCcomm::remainingData(0) == 0);

	// Following commands still work
	HostUsb::state().cdcWriteAvailable = 1024;
	CHECK(query("mock.value?;") == "[mock.value?|0]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "nothing is sent while usb is not connected") {
	HostUsb::state().mounted = false;
	CHECK(query("mock.value=3;") == "");
	CHECK(mock.value == 3); // Still executed

	HostUsb::state().mounted = true;
	CHECK(query("mock.value?;") == "[mock.value?|3]\n"); // Old reply is not sent late
}

TEST_CASE_FIXTURE(CdcFixture, "writes from other interfaces are mirrored as value updates") {
	MockCommandInterface other;
	other.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::set, 21);
	process();
	CHECK(HostUsb::state().cdcTx == "[mock.0.value?|21]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "reads from other interfaces are not mirrored") {
	MockCommandInterface other;
	other.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::get);
	process();
	CHECK(HostUsb::state().cdcTx == "");
}

TEST_CASE_FIXTURE(CdcFixture, "handler broadcasts are printed") {
	mock.broadcastCommandReply(CommandReply((int64_t)42), MockCommandHandler::id(MockCmd::value), CMDtype::get);
	cdc.batchDone(); // Broadcasts are flushed with the next batch or when the buffer is full
	process();
	CHECK(HostUsb::state().cdcTx == "[mock.0.value?|42]\n");
}

TEST_CASE_FIXTURE(CdcFixture, "incomplete input is discarded after 500ms") {
	receive("mock.garb");
	HostPlatform::advanceMs(501);
	CHECK(query("mock.value?;") == "[mock.value?|0]\n");
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).empty());
}

TEST_CASE_FIXTURE(CdcFixture, "interface description") {
	CHECK(cdc.getHelpstring().find("CDC interface") != std::string::npos);
	CHECK(cdc.bufferCapacity() > 0);
	CHECK(cdc.bufferCapacity() <= CDC_CMD_BUFFER_SIZE);
}

TEST_CASE_FIXTURE(CdcFixture, "ready to send depends on pending usb data") {
	CHECK(cdc.readyToSend());
	HostUsb::state().cdcConnected = false;
	CHECK(cdc.readyToSend()); // Data is discarded if no terminal is connected
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
namespace {

struct UartFixture : CommandSystemFixture {
	MockCommandHandler mock{"mock", CLSID_CUSTOM, 0};
	UART_CommandInterface uart{115200};

	/** Sends text byte by byte via the uart receive interrupt and returns everything the device replies */
	std::string query(const std::string &text) {
		HostPlatform::uart().tx.clear();
		HostPlatform::uartReceive(text);
		process();
		return HostPlatform::uart().tx;
	}
};

} // namespace

TEST_SUITE("UART_CommandInterface") {

TEST_CASE_FIXTURE(UartFixture, "port is configured and receiving after creation") {
	CHECK(HostPlatform::uart().initCount == 1);
	CHECK(huart1.Init.BaudRate == 115200);
	CHECK(HostPlatform::uart().rxArmedCount == 1);
	CHECK(external_uart.isReserved());
}

TEST_CASE_FIXTURE(UartFixture, "get command is answered on the uart") {
	mock.value = 99;
	CHECK(query("mock.value?;") == "[mock.value?|99]\n");
}

TEST_CASE_FIXTURE(UartFixture, "set command changes the value and is acknowledged") {
	CHECK(query("mock.value=17\n") == "[mock.value=17|OK]\n");
	CHECK(mock.value == 17);
}

TEST_CASE_FIXTURE(UartFixture, "no received byte is lost") {
	query("mock.addr=123?456;");
	CHECK(HostPlatform::uart().rxLost == 0);
	CHECK(mock.values[456] == 123);
}

TEST_CASE_FIXTURE(UartFixture, "several commands are answered in order") {
	CHECK(query("mock.value=1;mock.value?;mock.addr=5?2;mock.addr?2;") == "[mock.value=1|OK]\n[mock.value?|1]\n[mock.addr=5?2|OK]\n[mock.addr?2|5:2]\n");
	CHECK(HostPlatform::uart().rxLost == 0);
}

TEST_CASE_FIXTURE(UartFixture, "consecutive queries are answered") {
	CHECK(query("mock.value=1;") == "[mock.value=1|OK]\n");
	CHECK(query("mock.value?;") == "[mock.value?|1]\n");
	CHECK(query("mock.value=2;") == "[mock.value=2|OK]\n");
	CHECK(query("mock.value?;") == "[mock.value?|2]\n");
}

TEST_CASE_FIXTURE(UartFixture, "unknown commands are not answered but create an error") {
	CHECK(query("mock.nothere?;") == "");
	CHECK(errorsWithCode(ErrorCode::cmdNotFound).size() == 1);
}

TEST_CASE_FIXTURE(UartFixture, "commands of other interfaces and broadcasts are not sent on the uart") {
	MockCommandInterface other;
	other.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::set, 21);
	process();
	mock.broadcastCommandReply(CommandReply((int64_t)42), MockCommandHandler::id(MockCmd::value), CMDtype::get);
	uart.batchDone();
	process();
	CHECK(HostPlatform::uart().tx == "");
}

TEST_CASE_FIXTURE(UartFixture, "incomplete input is discarded after 2000ms") {
	HostPlatform::uartReceive("mock.garb");
	HostPlatform::advanceMs(2001);
	CHECK(query("mock.value?;") == "[mock.value?|0]\n");
}

TEST_CASE_FIXTURE(UartFixture, "not ready to send while a transfer is active") {
	CHECK(uart.readyToSend());
	external_uart.takeSemaphore(true);
	CHECK_FALSE(uart.readyToSend());
	external_uart.giveSemaphore(true);
	CHECK(uart.readyToSend());
}

TEST_CASE_FIXTURE(UartFixture, "cdc and uart interfaces work side by side") {
	CDC_CommandInterface cdc;
	CHECK(query("mock.value=5;") == "[mock.value=5|OK]\n");
	CHECK(HostUsb::state().cdcTx == "[mock.value?|5]\n"); // Mirrored as update on cdc

	std::string text = "mock.value=6;";
	uint32_t len = text.size();
	HostPlatform::uart().tx.clear();
	cdc.addBuf(text.data(), &len);
	process();
	CHECK(HostPlatform::uart().tx == ""); // Uart does not mirror
	CHECK(mock.value == 6);
}

TEST_CASE("uart port is released when the interface is destroyed") {
	FirmwareFixture fixture;
	{
		UART_CommandInterface uart{115200};
		CHECK(external_uart.isReserved());
	}
	CHECK_FALSE(external_uart.isReserved());
}

} // TEST_SUITE
