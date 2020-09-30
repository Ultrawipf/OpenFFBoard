/*
 * UartHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "UartHandler.h"
#include "global_callbacks.h"

UartHandler::UartHandler() {
	extern std::vector<UartHandler*> uartHandlers;
	addCallbackHandler(&uartHandlers, this);
}

UartHandler::~UartHandler() {
	extern std::vector<UartHandler*> uartHandlers;
	removeCallbackHandler(&uartHandlers, this);
}


void UartHandler::uartRcv(char* buf){

}
