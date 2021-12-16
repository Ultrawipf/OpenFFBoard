/*
 * CommandInterface.h
 *
 *  Created on: 17.11.2021
 *      Author: Yannick
 */

#ifndef SRC_COMMANDINTERFACE_H_
#define SRC_COMMANDINTERFACE_H_

#include "CmdParser.h"
#include "FFBoardMainCommandThread.h"
#include "UART.h"
#include "thread.hpp"
#include "CommandHandler.h"


class FFBoardMainCommandThread;

/**
 * Different command interface implementations
 */

class CommandInterface {
public:
	static std::vector<CommandInterface*> cmdInterfaces;

	CommandInterface();
	virtual ~CommandInterface();
	const virtual std::string getHelpstring(){return "";};
	virtual bool getNewCommands(std::vector<ParsedCommand>& commands) = 0;
	virtual bool hasNewCommands();
	virtual void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface); // All commands from batch done
protected:
	bool parserReady = false;
};

/**
 * Helper class for string based interfaces
 */
class StringCommandInterface : public CommandInterface{
public:
	StringCommandInterface(uint32_t reservedBuffer = 16) : parser(CmdParser(reservedBuffer)){}
	bool addBuf(char* Buf, uint32_t *Len);
	bool getNewCommands(std::vector<ParsedCommand>& commands) override;
	static void formatReply(std::string& reply,const std::vector<CommandResult>& results,const bool formatWriteAsRead=false);
	static std::string formatOriginalCommandFromResult(const ParsedCommand& originalCommand,CommandHandler* commandHandler,const bool formatWriteAsRead=false);
	static void generateReplyValueString(std::string& replyPart,const CommandReply& reply);
	static void generateReplyFromCmd(std::string& replyPart,const ParsedCommand& originalCommand);
	const std::string getHelpstring(){return "Syntax:\nGet: cls.(instance.)cmd? or cls.(instance.)cmd?adr\nSet: cls.(instance.)cmd=val or cls.(instance.)cmd=val?adr";};


protected:
	CmdParser parser; // String parser
};


//receives bytes from mainclass. calls its own parser instance, calls global parser thread, passes replies  back to cdc port.
class CDC_CommandInterface : public StringCommandInterface{
public:
	CDC_CommandInterface();
	virtual ~CDC_CommandInterface();

	void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface) override;
	const std::string getHelpstring(){return "CDC interface. " + StringCommandInterface::getHelpstring();};

private:
	bool enableBroadcastFromOtherInterfaces = true;
	const uint32_t parserTimeout = 2000;
};



class UART_CommandInterface : public StringCommandInterface,public UARTDevice, public cpp_freertos::Thread{
public:
	UART_CommandInterface(uint32_t baud = 115200);
	virtual ~UART_CommandInterface();

	void Run();

	void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface) override;
	void uartRcv(char& buf);
	const std::string getHelpstring(){return "UART interface. " + StringCommandInterface::getHelpstring();};
	//void endUartTransfer(UARTPort* port) override;

private:
	const uint32_t parserTimeout = 2000;

	UART_InitTypeDef uartconfig;
	uint32_t baud = 115200;
	std::string sendBuffer;
	bool enableBroadcastFromOtherInterfaces = false; // uart is slow. do not broadcast other messages by default
	cpp_freertos::BinarySemaphore cmdUartSem;
	cpp_freertos::BinarySemaphore bufferSem;
	std::vector<CommandResult> resultsBuffer;
	bool nextFormat = false;
};




#endif /* SRC_COMMANDINTERFACE_H_ */
