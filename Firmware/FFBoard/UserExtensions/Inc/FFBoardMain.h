/*
 * FFBoardMain.h
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#ifndef FFBOARDMAIN_H_
#define FFBOARDMAIN_H_

#include <CmdParser.h>
#include "cppmain.h"
#include "main.h"
#include <string>
#include "usbd_cdc_if.h"
#include "ChoosableClass.h"
#include "CommandHandler.h"
#include <vector>
#include "ErrorHandler.h"

class FFBoardMain : virtual ChoosableClass, public CommandHandler {
public:
	static ClassIdentifier info;
	virtual const ClassIdentifier getInfo();

	FFBoardMain();
	virtual ~FFBoardMain();

	virtual void usbInit(USBD_HandleTypeDef* hUsbDeviceFS); // initialize a composite usb device

	// Callbacks
	virtual void update();
	virtual void cdcRcv(char* Buf, uint32_t *Len);
	virtual void SOF();
	virtual void cdcFinished(); // Cdc send transfer complete
	virtual void usbSuspend(); // Called on usb disconnect and suspend
	virtual void usbResume(); // Called on usb resume
	virtual void updateSys();
	static void printFlashDump(std::string *reply);
	static void printErrors(std::string *reply);

	Error_t cmdNotFoundError = {
		code : ErrorCode::cmdNotFound,
		type : ErrorType::temporary,
		info : "Invalid command"
	};

	Error_t cmdExecError = {
		code : ErrorCode::cmdExecutionError,
		type : ErrorType::temporary,
		info : "Error while executing command"
	};
private:
	bool usb_busy_retry = false;
	std::string cmd_reply;

protected:
	virtual std::string getHelpstring(){return "\nSystem Commands: errors,reboot,help,dfu,swver (Version),hwtype,lsmain (List configs),id,main (Set main config),lsactive (print command handlers),vint,vext,format (Erase flash),mallinfo (Mem usage),flashdump,flashraw\n";}

	virtual void executeCommands(std::vector<ParsedCommand> commands);
	virtual ParseStatus command(ParsedCommand* cmd,std::string* reply); // Append reply strings to reply buffer
	virtual ParseStatus executeSysCommand(ParsedCommand* cmd,std::string* reply);
	CmdParser parser = CmdParser();
	bool parserReady = false;
};



#endif /* FFBOARDMAIN_H_ */
