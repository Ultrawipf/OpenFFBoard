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

#include "FFBoardMainCommandThread.h"

class FFBoardMainCommandThread;
class FFBoardMain : virtual ChoosableClass, public CommandHandler{
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

	static void sendSerial(std::string cmd,std::string string);
	static void logSerial(std::string* string);

	virtual void parserDone(std::string* reply, FFBoardMainCommandThread* parser);

	virtual ParseStatus command(ParsedCommand* cmd,std::string* reply); // Append reply strings to reply buffer

	virtual std::string getHelpstring();
	FFBoardMainCommandThread* systemCommands;
protected:
	bool usb_busy_retry = false;
};



#endif /* FFBOARDMAIN_H_ */
