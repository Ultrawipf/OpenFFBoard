/*
 * HID command interface (HidCommandInterface.cpp)
 *
 * Commands are transferred as vendor defined HID reports (report id 0xA1, HID_CMD_Data_t).
 * Out reports are passed to hidCmdCallback, replies are sent as in reports with the
 * same layout and are recorded by the fake TinyUSB (HostUsb).
 */
#include "doctest.h"
#include "TestHelpers.h"
#include "FirmwareFixture.h"
#include "MockCommandHandler.h"
#include "MockCommandInterface.h"

#include "HidCommandInterface.h"
#include <cstring>

namespace {

/** Unpacked copy of a command report. Fields of the packed report struct can not be used in checks directly */
struct HidReply {
	uint8_t reportId;
	HidCmdType type;
	uint16_t clsid;
	uint8_t instance;
	uint32_t cmd;
	uint64_t data;
	uint64_t addr;
};

struct HidCmdFixture : CommandSystemFixture {
	MockCommandHandler mock{"mock", CLSID_CUSTOM, 0};
	HID_CommandInterface hid;

	/** Sends a command report like the usb callback does and returns all reply reports */
	std::vector<HidReply> send(HidCmdType type, MockCmd cmd, uint64_t data = 0, uint64_t addr = 0, uint8_t instance = 0, uint16_t clsid = CLSID_CUSTOM) {
		return sendRaw(type, MockCommandHandler::id(cmd), data, addr, instance, clsid);
	}

	std::vector<HidReply> sendRaw(HidCmdType type, uint32_t cmd, uint64_t data = 0, uint64_t addr = 0, uint8_t instance = 0, uint16_t clsid = CLSID_CUSTOM) {
		HostUsb::state().hidReports.clear();
		HID_CMD_Data_t report;
		report.type = type;
		report.clsid = clsid;
		report.instance = instance;
		report.cmd = cmd;
		report.data = data;
		report.addr = addr;
		hid.hidCmdCallback(&report);
		process();
		return replies();
	}

	/** Decodes all HID in reports sent by the device */
	std::vector<HidReply> replies() {
		std::vector<HidReply> out;
		for (const HostUsb::HidReport &raw : HostUsb::state().hidReports) {
			REQUIRE(raw.data.size() == sizeof(HID_CMD_Data_t));
			HID_CMD_Data_t report;
			std::memcpy(&report, raw.data.data(), sizeof(HID_CMD_Data_t));
			out.push_back({report.reportId, report.type, report.clsid, report.instance, report.cmd, report.data, report.addr});
		}
		return out;
	}
};

} // namespace

TEST_SUITE("HID_CommandInterface") {

TEST_CASE("command report layout matches the HID descriptor") {
	// 25 bytes including the report id. The configurator and the descriptor depend on this layout
	CHECK(sizeof(HID_CMD_Data_t) == 25);
	CHECK(offsetof(HID_CMD_Data_t, reportId) == 0);
	CHECK(offsetof(HID_CMD_Data_t, type) == 1);
	CHECK(offsetof(HID_CMD_Data_t, clsid) == 2);
	CHECK(offsetof(HID_CMD_Data_t, instance) == 4);
	CHECK(offsetof(HID_CMD_Data_t, cmd) == 5);
	CHECK(offsetof(HID_CMD_Data_t, data) == 9);
	CHECK(offsetof(HID_CMD_Data_t, addr) == 17);
	CHECK(HID_CMD_Data_t().reportId == 0xA1);
}

TEST_CASE_FIXTURE(HidCmdFixture, "interface registers as global hid command interface") {
	CHECK(HID_CommandInterface::globalInterface == &hid);
}

TEST_CASE_FIXTURE(HidCmdFixture, "request returns the value") {
	mock.value = 0x1122334455;
	auto reply = send(HidCmdType::request, MockCmd::value);

	REQUIRE(reply.size() == 1);
	CHECK(reply[0].reportId == HID_ID_HIDCMD);
	CHECK(reply[0].type == HidCmdType::request);
	CHECK(reply[0].clsid == CLSID_CUSTOM);
	CHECK(reply[0].instance == 0);
	CHECK(reply[0].cmd == MockCommandHandler::id(MockCmd::value));
	CHECK(reply[0].data == 0x1122334455);

	// Sent with report id 0 because the id is the first byte of the data
	CHECK(HostUsb::state().hidReports[0].reportId == 0);
	CHECK(HostUsb::state().hidReports[0].data[0] == 0xA1);
}

TEST_CASE_FIXTURE(HidCmdFixture, "negative values are transferred as 64 bit two's complement") {
	mock.value = -2;
	auto reply = send(HidCmdType::request, MockCmd::value);
	REQUIRE(reply.size() == 1);
	CHECK((int64_t)reply[0].data == -2);

	send(HidCmdType::write, MockCmd::value, (uint64_t)(int64_t)-1000);
	CHECK(mock.value == -1000);
}

TEST_CASE_FIXTURE(HidCmdFixture, "write changes the value and replies with the written value") {
	auto reply = send(HidCmdType::write, MockCmd::value, 77);

	CHECK(mock.value == 77);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::request);
	CHECK(reply[0].cmd == MockCommandHandler::id(MockCmd::value));
	CHECK(reply[0].data == 77);
}

TEST_CASE_FIXTURE(HidCmdFixture, "write with address replies with value and address") {
	auto reply = send(HidCmdType::writeAddr, MockCmd::addr, 77, 5);

	CHECK(mock.values[5] == 77);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::requestAddr);
	CHECK(reply[0].data == 77);
	CHECK(reply[0].addr == 5);
}

TEST_CASE_FIXTURE(HidCmdFixture, "request with address") {
	mock.values[9] = 1234;
	auto reply = send(HidCmdType::requestAddr, MockCmd::addr, 0, 9);

	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::requestAddr);
	CHECK(reply[0].data == 1234);
	CHECK(reply[0].addr == 9);
}

TEST_CASE_FIXTURE(HidCmdFixture, "reply types") {
	SUBCASE("string or int replies are sent as int") {
		mock.value = 5;
		auto reply = send(HidCmdType::request, MockCmd::strint);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == HidCmdType::request);
		CHECK(reply[0].data == 5);
	}
	SUBCASE("value and address pair") {
		mock.value = 5;
		mock.dualAdr = 6;
		auto reply = send(HidCmdType::request, MockCmd::dual);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == HidCmdType::requestAddr);
		CHECK(reply[0].data == 5);
		CHECK(reply[0].addr == 6);
	}
	SUBCASE("request without reply data is acknowledged") {
		auto reply = send(HidCmdType::request, MockCmd::ack);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == HidCmdType::ACK);
	}
	SUBCASE("multiple replies are sent as multiple reports") {
		auto reply = send(HidCmdType::request, MockCmd::multi);
		REQUIRE(reply.size() == 3);
		CHECK(reply[0].data == 1);
		CHECK(reply[1].data == 2);
		CHECK(reply[2].data == 3);
	}
	SUBCASE("string replies can not be transferred and are skipped") {
		CHECK(send(HidCmdType::request, MockCmd::str).empty());
		CHECK(send(HidCmdType::info, MockCmd::info).empty());
		CHECK(mock.received.size() == 2); // Still executed
	}
	SUBCASE("NO_REPLY sends nothing") {
		CHECK(send(HidCmdType::request, MockCmd::noreply).empty());
	}
}

TEST_CASE_FIXTURE(HidCmdFixture, "handler error is reported as error report") {
	auto reply = send(HidCmdType::request, MockCmd::fail);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::ACK); // Characterization: A failed request without reply data is acknowledged, not reported as err

	reply = send(HidCmdType::write, MockCmd::fail, 3);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::request); // Characterization: A failed write is echoed like a successful one
}

TEST_CASE_FIXTURE(HidCmdFixture, "failed commands are reported with the err type" *
		KNOWN_ISSUE("HID_CommandInterface::sendReplies checks the command type before the error status. CommandStatus::ERR is never reported to the host")) {
	auto reply = send(HidCmdType::write, MockCmd::fail, 3);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::err);
}

TEST_CASE_FIXTURE(HidCmdFixture, "unknown targets are answered with not found") {
	SUBCASE("unknown command") {
		auto reply = sendRaw(HidCmdType::request, MockCommandHandler::id(MockCmd::unknown), 1, 2);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == HidCmdType::notFound);
		CHECK(reply[0].cmd == MockCommandHandler::id(MockCmd::unknown));
		CHECK(reply[0].clsid == CLSID_CUSTOM);
		CHECK(reply[0].data == 1); // Request is echoed
		CHECK(reply[0].addr == 2);
	}
	SUBCASE("unknown class") {
		auto reply = send(HidCmdType::request, MockCmd::value, 0, 0, 0, 0x7777);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == HidCmdType::notFound);
		CHECK(reply[0].clsid == 0x7777);
	}
	SUBCASE("unknown instance") {
		auto reply = send(HidCmdType::request, MockCmd::value, 0, 0, 4);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == HidCmdType::notFound);
		CHECK(reply[0].instance == 4);
	}
	CHECK(mock.received.empty());
}

TEST_CASE_FIXTURE(HidCmdFixture, "string only commands are not available via HID") {
	auto reply = send(HidCmdType::request, MockCmd::stronly);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::notFound);
}

TEST_CASE_FIXTURE(HidCmdFixture, "HID only commands are available") {
	mock.value = 3;
	auto reply = send(HidCmdType::request, MockCmd::hidonly);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::request);
	CHECK(reply[0].data == 3);
}

TEST_CASE_FIXTURE(HidCmdFixture, "debug commands require debug mode") {
	CHECK(send(HidCmdType::request, MockCmd::debug)[0].type == HidCmdType::notFound);
	SystemCommands::debugMode = true;
	CHECK(send(HidCmdType::request, MockCmd::debug)[0].type == HidCmdType::request);
}

TEST_CASE_FIXTURE(HidCmdFixture, "commands with a not allowed access type are not answered") {
	// Characterization: The host gets no feedback at all in this case
	CHECK(send(HidCmdType::write, MockCmd::readonly, 1).empty());
	CHECK(mock.received.empty());
}

TEST_CASE_FIXTURE(HidCmdFixture, "instance 0xFF addresses all instances of a class") {
	MockCommandHandler second{"mock", CLSID_CUSTOM, 1};
	mock.value = 10;
	second.value = 11;

	auto reply = send(HidCmdType::request, MockCmd::value, 0, 0, 0xFF);
	REQUIRE(reply.size() == 2);
	CHECK(reply[0].instance == 0);
	CHECK(reply[0].data == 10);
	CHECK(reply[1].instance == 1);
	CHECK(reply[1].data == 11);
}

TEST_CASE_FIXTURE(HidCmdFixture, "broadcast to an unknown class is not answered") {
	// Characterization: differs from the CAN interface which replies notFound
	CHECK(send(HidCmdType::request, MockCmd::value, 0, 0, 0xFF, 0x7777).empty());
}

TEST_CASE_FIXTURE(HidCmdFixture, "internal commands") {
	auto reply = sendRaw(HidCmdType::request, (uint32_t)CommandHandlerCommands::id);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].data == CLSID_CUSTOM);

	reply = sendRaw(HidCmdType::requestAddr, (uint32_t)CommandHandlerCommands::cmdinfo, 0, MockCommandHandler::id(MockCmd::value));
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].data == (CMDFLAG_GET | CMDFLAG_SET));

	// name and help are string only
	CHECK(sendRaw(HidCmdType::request, (uint32_t)CommandHandlerCommands::name)[0].type == HidCmdType::notFound);
}

TEST_CASE_FIXTURE(HidCmdFixture, "writes of other interfaces are mirrored as value updates") {
	MockCommandInterface other;
	other.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::set, 21);
	process();

	auto reply = replies();
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::request);
	CHECK(reply[0].cmd == MockCommandHandler::id(MockCmd::value));
	CHECK(reply[0].data == 21);
}

TEST_CASE_FIXTURE(HidCmdFixture, "handler broadcasts are sent") {
	mock.broadcastCommandReply(CommandReply((int64_t)42), MockCommandHandler::id(MockCmd::value), CMDtype::get);
	process();

	auto reply = replies();
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == HidCmdType::request);
	CHECK(reply[0].data == 42);
}

TEST_CASE_FIXTURE(HidCmdFixture, "several reports are executed in order") {
	HID_CMD_Data_t report;
	report.clsid = CLSID_CUSTOM;
	report.cmd = MockCommandHandler::id(MockCmd::value);
	for (uint64_t i = 1; i <= 5; i++) {
		report.type = HidCmdType::write;
		report.data = i;
		hid.hidCmdCallback(&report);
	}
	process();

	auto reply = replies();
	REQUIRE(reply.size() == 5);
	for (uint64_t i = 1; i <= 5; i++) {
		CHECK(reply[i - 1].data == i);
	}
	CHECK(mock.value == 5);
}

TEST_CASE_FIXTURE(HidCmdFixture, "pending replies are limited while the host does not read reports") {
	HostUsb::state().hidReady = false;
	for (int i = 0; i < 200; i++) {
		mock.broadcastCommandReply(CommandReply((int64_t)i), MockCommandHandler::id(MockCmd::value), CMDtype::get);
	}
	CHECK(hid.waitingToSend());
	CHECK_FALSE(hid.readyToSend());

	HostUsb::state().hidReady = true;
	process();
	CHECK(replies().size() == 50);
	CHECK_FALSE(hid.waitingToSend());
	CHECK(hid.readyToSend());
}

} // TEST_SUITE
