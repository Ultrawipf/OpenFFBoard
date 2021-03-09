/*
 * UartHandler.h
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#ifndef UARTHANDLER_H_
#define UARTHANDLER_H_

#include "cppmain.h"

class UartHandler {
public:
	static std::vector<UartHandler*> uartHandlers;

	UartHandler();
	virtual ~UartHandler();
	virtual void uartRcv(char* buf);
};

#endif /* UARTHANDLER_H_ */
