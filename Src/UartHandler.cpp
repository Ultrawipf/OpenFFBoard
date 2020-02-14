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
	uartHandlers.push_back(this);

}

UartHandler::~UartHandler() {
	extern std::vector<UartHandler*> uartHandlers;
	for (uint8_t i = 0; i < uartHandlers.size(); i++){
		if(uartHandlers[i] == this){
			uartHandlers.erase(uartHandlers.begin()+i);
			break;
		}
	}
}


void UartHandler::uartRcv(char* buf){

}
