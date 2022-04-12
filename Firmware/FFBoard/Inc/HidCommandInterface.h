/*
 * HidCommandInterface.h
 *
 *  Created on: 10.12.2021
 *      Author: Yannick
 */

#ifndef INC_HIDCOMMANDINTERFACE_H_
#define INC_HIDCOMMANDINTERFACE_H_
#include "CommandInterface.h"
#include "semaphore.hpp"
#include "thread.hpp"
#include "ffb_defs.h"
#include "CommandHandler.h"

enum class HidCmdType : uint8_t {write = 0, request = 1, info = 2, writeAddr = 3, requestAddr = 4,ACK = 10, notFound = 13, notification = 14, err = 15};



// TODO: can only be 25B long. tinyusb will hang otherwise!
typedef struct
{
	uint8_t		reportId = HID_ID_HIDCMD; // Report ID = 0xA1
	HidCmdType	type = HidCmdType::err;	// 0x01. Type of report. 0 = write, 1 = request
	uint16_t	clsid = 0;				// 0x02 Class ID identifies the target class type
	uint8_t 	instance = 0;			// 0x03 Class instance number to target a specific instance (often 0). 0xff for broadcast to all instances
	uint32_t	cmd = 0;				// 0x04 Use this as an identifier for the command. upper 16 bits for class type
	uint64_t	data = 0;				// 0x05 Use this to transfer data or the primary value
	uint64_t	addr = 0;				// 0x06 Use this to transfer an optional address or second value (CAN for example)

} __attribute__((packed)) HID_CMD_Data_t;



class HID_CommandInterface : public CommandInterface, public cpp_freertos::Thread{
public:
	HID_CommandInterface();
	virtual ~HID_CommandInterface();

	static HID_CommandInterface* globalInterface; // Pointer used to access the main hid command interface

	const std::string getHelpstring(){return "HID";}; // Not applicable here
	bool getNewCommands(std::vector<ParsedCommand>& commands);
	//bool hasNewCommands();
	void sendReplies(const std::vector<CommandResult>& results,CommandInterface* originalInterface); // All commands from batch done
	void hidCmdCallback(HID_CMD_Data_t* data);
	bool sendHidCmd(HID_CMD_Data_t* data);
	void queueReplyValues(const CommandReply& reply,const ParsedCommand& command);
	void transferComplete(uint8_t itf, uint8_t const* report, uint8_t len);
	bool readyToSend();
	void Run();
	bool waitingToSend();

private:
	std::vector<ParsedCommand> commands;
	std::vector<HID_CMD_Data_t> outBuffer;
	bool enableBroadcastFromOtherInterfaces = true; // TODO make configurable via command
	//static cpp_freertos::BinarySemaphore threadSem;
	const uint32_t maxQueuedReplies = 50;
	const uint32_t maxQueuedRepliesBroadcast = 10; // Must be smaller than maxQueuedReplies

};



#endif /* INC_HIDCOMMANDINTERFACE_H_ */
