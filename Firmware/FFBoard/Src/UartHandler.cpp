/*
 * UartHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "UartHandler.h"

UartHandler::UartHandler() {
	addCallbackHandler(getUARTHandlers(), this);
}

UartHandler::~UartHandler() {
	removeCallbackHandler(getUARTHandlers(), this);
}


void UartHandler::uartRxComplete(UART_HandleTypeDef *huart){

}


void UartHandler::uartTxComplete(UART_HandleTypeDef *huart){

}
