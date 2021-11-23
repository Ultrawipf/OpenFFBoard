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
	static std::string getHelpstring(){return "";}
	virtual std::vector<ParsedCommand> getNewCommands() = 0;
	virtual bool hasNewCommands();
	virtual void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface); // All commands from batch done

protected:
	bool parserReady = false;
};



//TODO receives bytes from mainclass. calls its own parser instance, calls global parser thread, passes replies  back to cdc port.
class CDC_CommandInterface : public CommandInterface{
public:
	CDC_CommandInterface();
	virtual ~CDC_CommandInterface();

	std::vector<ParsedCommand> getNewCommands() override;
	void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface) override;

	static std::string getHelpstring(){return "\nCDC interface. System Commands: errors,reboot,help,dfu,swver (Version),hwtype,minVerGui,lsmain (List configs),id,main (Set main config),lsactive (print command handlers),vint,vext,format (Erase flash),mallinfo,heapfree (Mem usage),flashdump,flashraw\n";}

	bool addBuf(char* Buf, uint32_t *Len);

private:
	CmdParser parser = CmdParser(); // String parser
};



class UART_CommandInterface : public CommandInterface,public UARTDevice{
public:
	UART_CommandInterface();
	virtual ~UART_CommandInterface();

	std::vector<ParsedCommand> getNewCommands() override;
	void sendReplies(std::vector<CommandResult>& results,CommandInterface* originalInterface) override;

	static std::string getHelpstring(){return "\nUART interface. System Commands: errors,reboot,help,dfu,swver (Version),hwtype,minVerGui,lsmain (List configs),id,main (Set main config),lsactive (print command handlers),vint,vext,format (Erase flash),mallinfo,heapfree (Mem usage),flashdump,flashraw\n";}

	bool addBuf(char* Buf, uint32_t *Len);
	void uartRcv(char& buf);

private:
	CmdParser parser = CmdParser(); // String parser

	UART_InitTypeDef uartconfig;
};



#endif /* SRC_COMMANDINTERFACE_H_ */
