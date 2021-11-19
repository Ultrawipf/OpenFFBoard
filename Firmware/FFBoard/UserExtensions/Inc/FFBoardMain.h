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
#include <memory>

#include "FFBoardMainCommandThread.h"
#include "USBdevice.h"
#include "CommandInterface.h"


class USBdevice;
class FFBoardMainCommandThread;
class CDC_CommandInterface;
class UART_CommandInterface;

class FFBoardMain : virtual ChoosableClass, public CommandHandler{
public:
	static ClassIdentifier info;
	virtual const ClassIdentifier getInfo();
	static bool isCreatable() {return true;};

	FFBoardMain();
	virtual ~FFBoardMain();

	virtual void usbInit(); // called before the mainloop to start the usb device

	// Callbacks
	virtual void update();
	virtual void cdcRcv(char* Buf, uint32_t *Len);
	//virtual void cdcFinished(uint8_t itf); // Cdc send transfer complete
	virtual void usbSuspend(); // Called on usb disconnect and suspend
	virtual void usbResume(); // Called on usb resume

	virtual ParseStatus command(ParsedCommand* cmd,std::string* reply); // Append reply strings to reply buffer


	virtual std::string getHelpstring();

	std::unique_ptr<FFBoardMainCommandThread> commandThread;
	std::unique_ptr<CDC_CommandInterface> cdcCmdInterface = std::make_unique<CDC_CommandInterface>();
	//std::unique_ptr<UART_CommandInterface> uartCmdInterface = std::make_unique<UART_CommandInterface>(); // UART command interface
protected:
	std::unique_ptr<USBdevice> usbdev;
};



#endif /* FFBOARDMAIN_H_ */
