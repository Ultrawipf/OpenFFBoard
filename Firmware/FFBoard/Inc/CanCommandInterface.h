/*
 * CanCommandInterface.h
 *
 *  Created on: 03.08.2026
 *      Author: Yannick
 */

#ifndef SRC_CANCOMMANDINTERFACE_H_
#define SRC_CANCOMMANDINTERFACE_H_
#include "target_constants.h"
#ifdef CANBUS

#include "CommandInterface.h"
#include "CommandHandler.h"
#include "CanHandler.h"
#include "CAN.h"
#include "thread.hpp"
#include <vector>

/**
 * Type of an incoming CAN command frame (header frame, baseId+0).
 * No RTR based variants and no info/string type - numerical values only.
 */
enum class CanCmdType : uint8_t {write = 0, request = 1, writeAddr = 2, requestAddr = 3};

/**
 * Type of an outgoing CAN reply frame (reply header frame, baseId+3).
 */
enum class CanReplyType : uint8_t {ack = 0, int_ = 1, doubleInt = 2, err = 3, notFound = 4};

/**
 * Fully assembled incoming command. POD, produced in ISR context (canRxPendCallback),
 * consumed in task context (getNewCommands). Must stay allocation-free.
 */
struct CanRawCommand{
	CanCmdType type = CanCmdType::request;
	uint16_t clsid = 0;
	uint8_t instance = 0xFF;
	uint32_t cmdId = 0;
	int64_t val = 0;
	int64_t adr = 0;
};

/**
 * CAN based command interface.
 *
 * Uses 6 CAN ids relative to a base id:
 * baseId+0: incoming command header (type,clsid,instance,cmd)
 * baseId+1: incoming command data (val)		- only for write/writeAddr
 * baseId+2: incoming command addr (adr)		- only for writeAddr/requestAddr
 * baseId+3: outgoing reply header (type,clsid,instance,cmd)
 * baseId+4: outgoing reply data (val)			- for int_/doubleInt replies
 * baseId+5: outgoing reply addr (adr)			- only for doubleInt replies
 *
 * Only one command may be in flight (accumulating) at a time. A new header frame
 * always resets any partial accumulation. No RTR frames are used anywhere.
 */
class CAN_CommandInterface : public CommandInterface, public CanHandler, public cpp_freertos::Thread{
public:
	CAN_CommandInterface(uint32_t baseId,CANPort& port);
	virtual ~CAN_CommandInterface();

	bool getNewCommands(std::vector<ParsedCommand>& commands) override;
	bool hasNewCommands() override;
	void sendReplies(const std::vector<CommandResult>& results,CommandInterface* originalInterface) override;
	bool readyToSend() override;
	void Run();

	const std::string getHelpstring(){return "CAN";};

	void canRxPendCallback(CANPort* port,CAN_rx_msg& msg) override; // *** Runs in CAN RX ISR context ***

private:
	CANPort& port;
	const uint32_t baseId;
	int32_t filterIdHeader = -1;
	int32_t filterIdData = -1;
	int32_t filterIdAddr = -1;

	// --- RX: ISR-owned accumulator. Fixed POD, no containers, no allocation ---
	struct{
		bool headerSeen = false;
		bool valSeen = false;
		bool adrSeen = false;
		CanCmdType type = CanCmdType::request;
		uint16_t clsid = 0;
		uint8_t instance = 0xFF;
		uint32_t cmdId = 0;
		int64_t val = 0;
		int64_t adr = 0;
	} accum;

	static bool needsVal(CanCmdType type);
	static bool needsAdr(CanCmdType type);
	void resetAccum();
	void dispatchAccum();

	// --- RX: fixed capacity single-producer(ISR)/single-consumer(task) ring buffer ---
	static constexpr size_t RX_QUEUE_CAP = 8; // power of two
	CanRawCommand rxQueue[RX_QUEUE_CAP];
	volatile size_t rxHead = 0; // consumer (task) owned
	volatile size_t rxTail = 0; // producer (ISR) owned
	volatile uint32_t rxDroppedCount = 0; // diagnostic counter, best effort

	// --- TX: producer = sendReplies()/getNewCommands() (both task context, via FFBoardMainCommandThread), consumer = Run() (this thread) ---
	std::vector<CAN_tx_msg> outBuffer;
	const uint32_t maxQueuedReplies = 30;
	bool enableBroadcastFromOtherInterfaces = false; // CAN bandwidth is limited, do not mirror other interfaces by default

	void queueReplyValues(const CommandReply& reply,const ParsedCommand& command);
	void queueNotFound(uint16_t clsid,uint8_t instance,uint32_t cmdId);
	void pushTxFrame(uint32_t id,const uint8_t* data,uint32_t len);
};

#endif
#endif /* SRC_CANCOMMANDINTERFACE_H_ */
