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

class FFBoardMainCommandThread;


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
	bool addBuf(char* Buf, uint32_t *Len);
	bool getNewCommands(std::vector<ParsedCommand>& commands) override;
	static void formatReply(std::string& reply,const std::vector<CommandResult>& results,const bool formatWriteAsRead=false);
	static std::string formatOriginalCommandFromResult(const ParsedCommand& originalCommand,CommandHandler* commandHandler,const bool formatWriteAsRead=false);
	static void generateReplyValueString(std::string& replyPart,const CommandReply& reply);
	const std::string getHelpstring(){return "Syntax:\nGet: cls.(instance.)cmd? or cls.(instance.)cmd?adr\nSet: cls.(instance.)cmd=val or cls.(instance.)cmd=val?adr";};
private:
	CmdParser parser = CmdParser(); // String parser
};


//TODO receives bytes from mainclass. calls its own parser instance, calls global parser thread, passes replies  back to cdc port.
class CDC_CommandInterface : public StringCommandInterface{
public:
	CDC_CommandInterface();
	virtual ~CDC_CommandInterface();

	void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface) override;
	const std::string getHelpstring(){return "CDC interface. " + StringCommandInterface::getHelpstring();};

};



class UART_CommandInterface : public StringCommandInterface,public UARTDevice{
public:
	UART_CommandInterface();
	virtual ~UART_CommandInterface();

	//static std::string getHelpstring(){return "\nUART interface. System Commands: errors,reboot,help,dfu,swver (Version),hwtype,minVerGui,lsmain (List configs),id,main (Set main config),lsactive (print command handlers),vint,vext,format (Erase flash),mallinfo,heapfree (Mem usage),flashdump,flashraw\n";}
	void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface) override;
	void uartRcv(char& buf);
	const std::string getHelpstring(){return "UART interface. " + StringCommandInterface::getHelpstring();};

private:
	UART_InitTypeDef uartconfig;
};



#endif /* SRC_COMMANDINTERFACE_H_ */
