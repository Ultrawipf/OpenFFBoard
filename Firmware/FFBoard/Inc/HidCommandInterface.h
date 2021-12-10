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
#include "hid_cmd_defs.h"
#include "thread.hpp"

class HID_CommandInterface : public CommandInterface, public cpp_freertos::Thread{
public:
	HID_CommandInterface();
	virtual ~HID_CommandInterface();

	static HID_CommandInterface* globalInterface; // Pointer used to access the main hid command interface

	const std::string getHelpstring(){return "HID interface";};
	bool getNewCommands(std::vector<ParsedCommand>& commands);
	//bool hasNewCommands();
	void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface); // All commands from batch done
	void hidCmdCallback(HID_CMD_Data_t* data);
	bool sendHidCmd(HID_CMD_Data_t* data);
	void queueReplyValues(CommandReply& reply,ParsedCommand& command);
	void transferComplete(uint8_t itf, uint8_t const* report, uint8_t len);

	void Run();
private:
	std::vector<ParsedCommand> commands;
	std::vector<HID_CMD_Data_t> outBuffer;
	bool enableBroadcastFromOtherInterfaces = false;
	static cpp_freertos::BinarySemaphore threadSem;


};



#endif /* INC_HIDCOMMANDINTERFACE_H_ */
