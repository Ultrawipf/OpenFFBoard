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

class FFBoardMain : public ChoosableClass, CommandHandler {
public:
	static ClassIdentifier info;
	virtual const ClassIdentifier getInfo();

	FFBoardMain();
	virtual ~FFBoardMain();

	virtual void usbInit(); // initialize a composite usb device

	// Callbacks
	virtual void update();
	virtual void cdcRcv(char* Buf, uint32_t *Len);
	virtual void SOF();



private:

protected:
	virtual void executeCommands(std::vector<ParsedCommand> commands);
	virtual bool command(ParsedCommand* cmd,std::string* reply); // Append reply strings to reply buffer
	virtual bool executeSysCommand(ParsedCommand* cmd,std::string* reply);
	CmdParser parser = CmdParser();
};



#endif /* FFBOARDMAIN_H_ */
