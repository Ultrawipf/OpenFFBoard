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
#include "ChoosableClass.h"
#include "CommandHandler.h"
#include <vector>
#include "ErrorHandler.h"

#include "FFBoardMainCommandThread.h"
#include "USBdevice.h"


class USBdevice;
class FFBoardMainCommandThread;

class FFBoardMain : virtual ChoosableClass, public CommandHandler{
public:
	static ClassIdentifier info;
	virtual const ClassIdentifier getInfo();

	FFBoardMain();
	virtual ~FFBoardMain();

	virtual void usbInit(); // called before the mainloop to start the usb device

	// Callbacks
	virtual void update();
	virtual void cdcRcv(char* Buf, uint32_t *Len);
	virtual void cdcFinished(uint8_t itf); // Cdc send transfer complete
	virtual void usbSuspend(); // Called on usb disconnect and suspend
	virtual void usbResume(); // Called on usb resume

	static void sendSerial(std::string cmd,std::string string);
	static void logSerial(std::string* string);
	uint16_t cdcSend(std::string* reply, std::string* remaining,uint8_t itf = 0);// sends raw data via cdc. returns what was not sent as substring

	virtual void parserDone(std::string* reply, FFBoardMainCommandThread* parser);

	virtual ParseStatus command(ParsedCommand* cmd,std::string* reply); // Append reply strings to reply buffer


	virtual std::string getHelpstring();
	FFBoardMainCommandThread* systemCommands;
protected:
	bool usb_busy_retry = false;
	std::string cdcRemaining;

	USBdevice* usbdev;
};



#endif /* FFBOARDMAIN_H_ */
