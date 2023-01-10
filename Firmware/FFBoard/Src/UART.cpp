/*
 * UART.cpp
 *
 *  Created on: May 4, 2021
 *      Author: Yannick
 */

#include "UART.h"
#include "semaphore.hpp"
#include "cppmain.h"

static bool operator==(const UART_InitTypeDef& lhs, const UART_InitTypeDef& rhs) {
	return lhs.BaudRate == rhs.BaudRate &&
	lhs.HwFlowCtl == rhs.HwFlowCtl &&
	lhs.Mode == rhs.Mode &&
	lhs.OverSampling == rhs.OverSampling &&
	lhs.Parity == rhs.Parity &&
	lhs.StopBits == rhs.StopBits &&
	lhs.WordLength == rhs.WordLength;
}


/*
 * Warning: For DMA a DMA stream must be configured in stm32cube and might conflict with other DMA streams
 * For interrupts and interrupt must be configured and reenabled after each receive (automatic)
 */
UARTPort::UARTPort(UART_HandleTypeDef& huart) : huart{huart} {
	// Do not enable the interrupt just yet.
}

UARTPort::~UARTPort() {

}

bool UARTPort::reconfigurePort(UART_InitTypeDef& config){
	if(this->huart.Init == config){
		return false;
	}

	this->huart.Init = config;

	if(HAL_UART_Init(&this->huart) != HAL_OK){
		return false;
	}

	return true;
}

UART_InitTypeDef& UARTPort::getConfig(){
	return huart.Init;
}

/**
 * Begins to wait on a single UART_BUF_SIZE sized transfer
 */
void UARTPort::registerInterrupt(){
	HAL_UART_Receive_IT(&this->huart,(uint8_t*)this->uart_buf,UART_BUF_SIZE);
}


void UARTPort::transmit_DMA(const char* txbuf,uint16_t size){
	if(this->device)
		device->startUartTransfer(this,true);
	HAL_UART_Transmit_DMA(&this->huart, (uint8_t*)(txbuf), size);
	// Transfer ends in txinterrupt
}

void UARTPort::transmit(const char* txbuf,uint16_t size,uint32_t timeout){
	if(this->device)
		device->startUartTransfer(this,true);
	HAL_UART_Transmit(&this->huart, (uint8_t*)(txbuf), size,timeout);
	if(this->device)
		device->endUartTransfer(this,true);
}

void UARTPort::transmit_IT(const char* txbuf,uint16_t size){
	if(this->device)
		device->startUartTransfer(this,true);
	HAL_UART_Transmit_IT(&this->huart, (uint8_t*)(txbuf), size);
	// Transfer ends in txinterrupt
}

bool UARTPort::receive(char* rxbuf,uint16_t size,uint32_t timeout){
	return(HAL_UART_Receive(&this->huart, (uint8_t*)(rxbuf), size, timeout) == HAL_OK);
}

bool UARTPort::receiveDMA(char* rxbuf,uint16_t size){
	if(this->device)
		device->startUartTransfer(this,false);
	return(HAL_UART_Receive_DMA(&this->huart, (uint8_t*)(rxbuf), size) == HAL_OK);
}

bool UARTPort::receiveIT(char* rxbuf,uint16_t size){
	if(this->device)
		device->startUartTransfer(this,false);
	return(HAL_UART_Receive_IT(&this->huart, (uint8_t*)(rxbuf), size) == HAL_OK);
}

bool UARTPort::reservePort(UARTDevice* device){
	if(this->device == nullptr || device == this->device){
		this->device = device;
		return true;
	}
	return false;
}

bool UARTPort::freePort(UARTDevice* device){
	if(device == this->device){
		this->device = nullptr;
		return true;
	}
	return false;
}

void UARTPort::takeSemaphore(){
	bool isIsr = inIsr();
	BaseType_t taskWoken = 0;
	if(isIsr)
		this->semaphore.TakeFromISR(&taskWoken);
	else
		this->semaphore.Take();
	isTakenFlag = true;
	portYIELD_FROM_ISR(taskWoken);
}

void UARTPort::giveSemaphore(){
	bool isIsr = inIsr();
	BaseType_t taskWoken = 0;
	if(isIsr)
		this->semaphore.GiveFromISR(&taskWoken);
	else
		this->semaphore.Give();
	isTakenFlag = false;
	portYIELD_FROM_ISR(taskWoken);
}

bool UARTPort::isTaken(){
	return isTakenFlag;
}


void UARTPort::uartRxComplete(UART_HandleTypeDef *huart){
	// Check if port matches this port
	if(huart == &this->huart){
		if(this->device != nullptr){
			device->uartRcv((char&)*this->uart_buf);
			if(isTakenFlag)
				device->endUartTransfer(this,true);
		}
	} // If started by interrupt we need to restart the interrupt transfer again by calling registerInterrupt()
}

void UARTPort::uartTxComplete(UART_HandleTypeDef *huart){
	// Check if port matches this port
	if(huart == &this->huart){
		if(device){
			device->endUartTransfer(this,true);
		}
	}
}


// UART Device

UARTDevice::UARTDevice(){

}

UARTDevice::UARTDevice(UARTPort& port) : uartport{&port}{
	uartport->reservePort(this);
}

UARTDevice::~UARTDevice(){
	uartport->freePort(this);
}


void UARTDevice::startUartTransfer(UARTPort* port,bool transmit){
	port->takeSemaphore();
}

void UARTDevice::endUartTransfer(UARTPort* port,bool transmit){
	port->giveSemaphore();
}


