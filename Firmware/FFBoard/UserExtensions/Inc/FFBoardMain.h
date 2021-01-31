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
	virtual void printFlashDump(std::string *reply);

private:
	bool usb_busy_retry = false;
	std::string cmd_reply;

protected:
	virtual std::string getHelpstring(){return "\nSystem Commands: reboot,help,dfu,swver (Version),hwtype,lsmain (List configs),id,main (Set main config),lsactive (print command handlers),vint,vext,format (Erase flash),mallinfo (Mem usage),flashdump,flashraw\n";}

	virtual void executeCommands(std::vector<ParsedCommand> commands);
	virtual ParseStatus command(ParsedCommand* cmd,std::string* reply); // Append reply strings to reply buffer
	virtual ParseStatus executeSysCommand(ParsedCommand* cmd,std::string* reply);
	CmdParser parser = CmdParser();
	bool parserReady = false;
};



#endif /* FFBOARDMAIN_H_ */
