/*
 * UartHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "UartHandler.h"
#include "global_callbacks.h"
std::vector<UartHandler*> UartHandler::uartHandlers;

UartHandler::UartHandler() {
	addCallbackHandler(uartHandlers, this);
}

UartHandler::~UartHandler() {
	removeCallbackHandler(uartHandlers, this);
}


void UartHandler::uartRcv(char* buf){

}
