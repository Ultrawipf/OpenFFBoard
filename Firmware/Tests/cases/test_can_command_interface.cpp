/*
 * CAN command interface (CanCommandInterface.cpp)
 *
 * Protocol relative to a base id:
 *   base+0  in   header: [type, clsid lo, clsid hi, instance, cmd (4 bytes LE)]
 *   base+1  in   value (8 bytes LE)    for write/writeAddr
 *   base+2  in   address (8 bytes LE)  for writeAddr/requestAddr
 *   base+3  out  reply header: [type, clsid lo, clsid hi, instance, cmd]
 *   base+4  out  reply value            for int/doubleInt replies
 *   base+5  out  reply address          for doubleInt replies
 *
 * Frames are injected and recorded with MockCANPort.
 */
#include "doctest.h"
#include "TestHelpers.h"
#include "FirmwareFixture.h"
#include "MockCommandHandler.h"
#include "MockCommandInterface.h"
#include "MockCANPort.h"

#include "CanCommandInterface.h"
#include <array>
#include <cstring>

namespace {

constexpr uint32_t BASE = 0x100;

struct CanReply {
	CanReplyType type;
	uint16_t clsid = 0;
	uint8_t instance = 0;
	uint32_t cmd = 0;
	bool hasVal = false;
	bool hasAdr = false;
	int64_t val = 0;
	int64_t adr = 0;
};

std::array<uint8_t, 8> header(CanCmdType type, uint32_t cmd, uint8_t instance = 0, uint16_t clsid = CLSID_CUSTOM) {
	std::array<uint8_t, 8> frame{};
	frame[0] = static_cast<uint8_t>(type);
	std::memcpy(&frame[1], &clsid, 2);
	frame[3] = instance;
	std::memcpy(&frame[4], &cmd, 4);
	return frame;
}

std::array<uint8_t, 8> payload(int64_t value) {
	std::array<uint8_t, 8> frame{};
	std::memcpy(frame.data(), &value, 8);
	return frame;
}

struct CanCmdFixture : CommandSystemFixture {
	MockCommandHandler mock{"mock", CLSID_CUSTOM, 0};
	MockCANPort port;
	CAN_CommandInterface can{BASE, port};

	/** Sends the frames of a complete command and returns the decoded replies */
	std::vector<CanReply> send(CanCmdType type, MockCmd cmd, int64_t val = 0, int64_t adr = 0, uint8_t instance = 0, uint16_t clsid = CLSID_CUSTOM) {
		return sendRaw(type, MockCommandHandler::id(cmd), val, adr, instance, clsid);
	}

	std::vector<CanReply> sendRaw(CanCmdType type, uint32_t cmd, int64_t val = 0, int64_t adr = 0, uint8_t instance = 0, uint16_t clsid = CLSID_CUSTOM) {
		port.sent.clear();
		port.receive(BASE, header(type, cmd, instance, clsid));
		if (type == CanCmdType::write || type == CanCmdType::writeAddr) {
			port.receive(BASE + 1, payload(val));
		}
		if (type == CanCmdType::writeAddr || type == CanCmdType::requestAddr) {
			port.receive(BASE + 2, payload(adr));
		}
		process();
		return replies();
	}

	/** Decodes the sent frames into replies. Checks the frame sequence */
	std::vector<CanReply> replies() {
		std::vector<CanReply> out;
		for (const CAN_tx_msg &msg : port.sent) {
			CHECK(msg.header.length == 8);
			CHECK_FALSE(msg.header.extId);
			CHECK_FALSE(msg.header.rtr);
			if (msg.header.id == BASE + 3) {
				CanReply reply;
				reply.type = static_cast<CanReplyType>(msg.data[0]);
				std::memcpy(&reply.clsid, &msg.data[1], 2);
				reply.instance = msg.data[3];
				std::memcpy(&reply.cmd, &msg.data[4], 4);
				out.push_back(reply);
			} else if (msg.header.id == BASE + 4) {
				REQUIRE_FALSE(out.empty()); // Value must follow a header
				CHECK_FALSE(out.back().hasVal);
				out.back().hasVal = true;
				std::memcpy(&out.back().val, msg.data, 8);
			} else if (msg.header.id == BASE + 5) {
				REQUIRE_FALSE(out.empty());
				CHECK_FALSE(out.back().hasAdr);
				out.back().hasAdr = true;
				std::memcpy(&out.back().adr, msg.data, 8);
			} else {
				FAIL("Frame with unexpected id sent: " << msg.header.id);
			}
		}
		return out;
	}
};

} // namespace

TEST_SUITE("CAN_CommandInterface") {

TEST_CASE_FIXTURE(CanCmdFixture, "interface starts the port and listens on three ids") {
	CHECK(port.started);
	CHECK(port.getPortUsers() == 1);
	CHECK(port.activeFilters() == 3);
	CHECK(port.passesFilter(BASE));
	CHECK(port.passesFilter(BASE + 1));
	CHECK(port.passesFilter(BASE + 2));
	CHECK_FALSE(port.passesFilter(BASE + 3));
	CHECK_FALSE(port.passesFilter(BASE - 1));
	CHECK_FALSE(port.passesFilter(BASE, true)); // No extended ids
}

TEST_CASE("interface releases the port and its filters when destroyed") {
	CommandSystemFixture fixture;
	MockCANPort port;
	{
		CAN_CommandInterface can{BASE, port};
		CHECK(port.activeFilters() == 3);
	}
	CHECK(port.activeFilters() == 0);
	CHECK(port.getPortUsers() == 0);
	CHECK_FALSE(port.started);
}

TEST_CASE_FIXTURE(CanCmdFixture, "request returns the value") {
	mock.value = 0x1122334455;
	auto reply = send(CanCmdType::request, MockCmd::value);

	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == CanReplyType::int_);
	CHECK(reply[0].clsid == CLSID_CUSTOM);
	CHECK(reply[0].instance == 0);
	CHECK(reply[0].cmd == MockCommandHandler::id(MockCmd::value));
	CHECK(reply[0].hasVal);
	CHECK_FALSE(reply[0].hasAdr);
	CHECK(reply[0].val == 0x1122334455);
	CHECK(port.sent.size() == 2); // Header and value frame
}

TEST_CASE_FIXTURE(CanCmdFixture, "write changes the value and replies with the written value") {
	auto reply = send(CanCmdType::write, MockCmd::value, -77);

	CHECK(mock.value == -77);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == CanReplyType::int_);
	CHECK(reply[0].val == -77);
}

TEST_CASE_FIXTURE(CanCmdFixture, "write is not executed before the value frame arrives") {
	port.receive(BASE, header(CanCmdType::write, MockCommandHandler::id(MockCmd::value)));
	process();
	CHECK(mock.received.empty());
	CHECK_FALSE(can.hasNewCommands());

	port.receive(BASE + 1, payload(5));
	CHECK(can.hasNewCommands());
	process();
	CHECK(mock.value == 5);
}

TEST_CASE_FIXTURE(CanCmdFixture, "write with address replies with value and address") {
	auto reply = send(CanCmdType::writeAddr, MockCmd::addr, 77, 5);

	CHECK(mock.values[5] == 77);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == CanReplyType::doubleInt);
	CHECK(reply[0].hasVal);
	CHECK(reply[0].hasAdr);
	CHECK(reply[0].val == 77);
	CHECK(reply[0].adr == 5);
	CHECK(port.sent.size() == 3);
}

TEST_CASE_FIXTURE(CanCmdFixture, "value and address frames may arrive in any order") {
	port.receive(BASE, header(CanCmdType::writeAddr, MockCommandHandler::id(MockCmd::addr)));
	port.receive(BASE + 2, payload(8));
	process();
	CHECK(mock.received.empty());
	port.receive(BASE + 1, payload(99));
	process();
	CHECK(mock.values[8] == 99);
}

TEST_CASE_FIXTURE(CanCmdFixture, "request with address") {
	mock.values[9] = 1234;
	auto reply = send(CanCmdType::requestAddr, MockCmd::addr, 0, 9);

	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == CanReplyType::doubleInt);
	CHECK(reply[0].val == 1234);
	CHECK(reply[0].adr == 9);
}

TEST_CASE_FIXTURE(CanCmdFixture, "reply types") {
	SUBCASE("string or int replies are sent as int") {
		mock.value = 5;
		auto reply = send(CanCmdType::request, MockCmd::strint);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == CanReplyType::int_);
		CHECK(reply[0].val == 5);
	}
	SUBCASE("request without reply data is acknowledged with a single frame") {
		auto reply = send(CanCmdType::request, MockCmd::ack);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == CanReplyType::ack);
		CHECK(port.sent.size() == 1);
	}
	SUBCASE("multiple replies") {
		auto reply = send(CanCmdType::request, MockCmd::multi);
		REQUIRE(reply.size() == 3);
		CHECK(reply[0].val == 1);
		CHECK(reply[1].val == 2);
		CHECK(reply[2].val == 3);
	}
	SUBCASE("string replies can not be transferred and are skipped") {
		CHECK(send(CanCmdType::request, MockCmd::str).empty());
		CHECK(mock.received.size() == 1);
	}
	SUBCASE("NO_REPLY sends nothing") {
		CHECK(send(CanCmdType::request, MockCmd::noreply).empty());
	}
}

TEST_CASE_FIXTURE(CanCmdFixture, "failed commands are reported with the err type" *
		KNOWN_ISSUE("CAN_CommandInterface::sendReplies checks the command type before the error status. CommandStatus::ERR is never reported")) {
	auto reply = send(CanCmdType::write, MockCmd::fail, 3);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].type == CanReplyType::err);
}

TEST_CASE_FIXTURE(CanCmdFixture, "unknown targets are answered with not found") {
	SUBCASE("unknown command") {
		auto reply = sendRaw(CanCmdType::request, MockCommandHandler::id(MockCmd::unknown));
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == CanReplyType::notFound);
		CHECK(reply[0].cmd == MockCommandHandler::id(MockCmd::unknown));
		CHECK(reply[0].clsid == CLSID_CUSTOM);
	}
	SUBCASE("unknown class") {
		auto reply = send(CanCmdType::request, MockCmd::value, 0, 0, 0, 0x7777);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == CanReplyType::notFound);
		CHECK(reply[0].clsid == 0x7777);
	}
	SUBCASE("unknown instance") {
		auto reply = send(CanCmdType::request, MockCmd::value, 0, 0, 4);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == CanReplyType::notFound);
		CHECK(reply[0].instance == 4);
	}
	SUBCASE("broadcast to unknown class") {
		auto reply = send(CanCmdType::request, MockCmd::value, 0, 0, 0xFF, 0x7777);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == CanReplyType::notFound);
	}
	SUBCASE("string only command") {
		auto reply = send(CanCmdType::request, MockCmd::stronly);
		REQUIRE(reply.size() == 1);
		CHECK(reply[0].type == CanReplyType::notFound);
	}
	CHECK(mock.received.empty());
}

TEST_CASE_FIXTURE(CanCmdFixture, "instance 0xFF addresses all instances of a class") {
	MockCommandHandler second{"mock", CLSID_CUSTOM, 1};
	mock.value = 10;
	second.value = 11;

	auto reply = send(CanCmdType::request, MockCmd::value, 0, 0, 0xFF);
	REQUIRE(reply.size() == 2);
	CHECK(reply[0].instance == 0);
	CHECK(reply[0].val == 10);
	CHECK(reply[1].instance == 1);
	CHECK(reply[1].val == 11);
}

TEST_CASE_FIXTURE(CanCmdFixture, "malformed frames are ignored") {
	std::array<uint8_t, 8> head = header(CanCmdType::request, MockCommandHandler::id(MockCmd::value));

	SUBCASE("short header") {
		port.receive(BASE, head.data(), 7);
	}
	SUBCASE("remote frame") {
		port.receive(BASE, head.data(), 8, true);
	}
	SUBCASE("value frame without header") {
		port.receive(BASE + 1, payload(5));
	}
	SUBCASE("address frame without header") {
		port.receive(BASE + 2, payload(5));
	}
	SUBCASE("short value frame") {
		port.receive(BASE, header(CanCmdType::write, MockCommandHandler::id(MockCmd::value)));
		port.receive(BASE + 1, payload(5).data(), 4);
	}
	SUBCASE("frame with other id") {
		CHECK_FALSE(port.receive(BASE + 6, head)); // Rejected by the filters
	}
	process();
	CHECK(mock.received.empty());
	CHECK(port.sent.empty());
}

TEST_CASE_FIXTURE(CanCmdFixture, "frames of another port are ignored") {
	MockCANPort otherPort;
	CAN_filter all;
	all.filter_mask = 0;
	otherPort.addCanFilter(all);

	otherPort.receive(BASE, header(CanCmdType::request, MockCommandHandler::id(MockCmd::value)));
	process();
	CHECK(mock.received.empty());
}

TEST_CASE_FIXTURE(CanCmdFixture, "a new header discards an incomplete command") {
	port.receive(BASE, header(CanCmdType::writeAddr, MockCommandHandler::id(MockCmd::addr)));
	port.receive(BASE + 1, payload(1));
	// Address never arrives. New command starts
	port.receive(BASE, header(CanCmdType::write, MockCommandHandler::id(MockCmd::value)));
	port.receive(BASE + 1, payload(42));
	process();

	REQUIRE(mock.received.size() == 1);
	CHECK(mock.value == 42);
	CHECK(mock.values.empty());
}

TEST_CASE_FIXTURE(CanCmdFixture, "several commands received before execution are executed in order") {
	for (int64_t i = 1; i <= 5; i++) {
		port.receive(BASE, header(CanCmdType::write, MockCommandHandler::id(MockCmd::value)));
		port.receive(BASE + 1, payload(i));
	}
	process();
	REQUIRE(mock.received.size() == 5);
	for (int64_t i = 1; i <= 5; i++) {
		CHECK(mock.received[i - 1].val == i);
	}
	CHECK(replies().size() == 5);
}

TEST_CASE_FIXTURE(CanCmdFixture, "commands exceeding the receive queue are dropped") {
	// Queue capacity is 8 with one slot kept free
	for (int64_t i = 1; i <= 20; i++) {
		port.receive(BASE, header(CanCmdType::write, MockCommandHandler::id(MockCmd::value)));
		port.receive(BASE + 1, payload(i));
	}
	process();
	CHECK(mock.received.size() == 7);
	CHECK(mock.value == 7); // Oldest commands are kept

	// Interface keeps working afterwards
	send(CanCmdType::write, MockCmd::value, 100);
	CHECK(mock.value == 100);
}

TEST_CASE_FIXTURE(CanCmdFixture, "commands of other interfaces and broadcasts are not sent on CAN") {
	MockCommandInterface other;
	other.queueCommand(mock, MockCommandHandler::id(MockCmd::value), CMDtype::set, 21);
	process();
	mock.broadcastCommandReply(CommandReply((int64_t)42), MockCommandHandler::id(MockCmd::value), CMDtype::get);
	process();
	CHECK(port.sent.empty());
}

TEST_CASE_FIXTURE(CanCmdFixture, "commands with a not allowed access type are not answered") {
	// Characterization: The host gets no feedback at all in this case
	CHECK(send(CanCmdType::write, MockCmd::readonly, 1).empty());
	CHECK(mock.received.empty());
}

TEST_CASE_FIXTURE(CanCmdFixture, "internal commands") {
	auto reply = sendRaw(CanCmdType::request, (uint32_t)CommandHandlerCommands::id);
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].val == CLSID_CUSTOM);

	reply = sendRaw(CanCmdType::requestAddr, (uint32_t)CommandHandlerCommands::cmdinfo, 0, MockCommandHandler::id(MockCmd::value));
	REQUIRE(reply.size() == 1);
	CHECK(reply[0].val == (CMDFLAG_GET | CMDFLAG_SET));
}

} // TEST_SUITE
