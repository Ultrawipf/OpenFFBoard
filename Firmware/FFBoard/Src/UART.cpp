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
bool UARTPort::registerInterrupt(){
	waitingForSingleBytes = true;
	return HAL_UART_Receive_IT(&this->huart,(uint8_t*)this->uart_buf,UART_BUF_SIZE) == HAL_OK;
}


bool UARTPort::transmit_DMA(const char* txbuf,uint16_t size){
	if(this->device)
		device->startUartTransfer(this,true);
	return HAL_UART_Transmit_DMA(&this->huart, (uint8_t*)(txbuf), size) == HAL_OK;
	// Transfer ends in txinterrupt
}

bool UARTPort::transmit(const char* txbuf,uint16_t size,uint32_t timeout){
	if(this->device)
		device->startUartTransfer(this,true);
	uint32_t status = HAL_UART_Transmit(&this->huart, (uint8_t*)(txbuf), size,timeout);
	if(this->device)
		device->endUartTransfer(this,true);
	return status == HAL_OK;
}

bool UARTPort::transmit_IT(const char* txbuf,uint16_t size){
	if(this->device)
		device->startUartTransfer(this,true);
	return HAL_UART_Transmit_IT(&this->huart, (uint8_t*)(txbuf), size) == HAL_OK;
	// Transfer ends in txinterrupt
}

bool UARTPort::receive(char* rxbuf,uint16_t size,uint32_t timeout){
	if(this->device)
		device->startUartTransfer(this,false);
	uint32_t status = (HAL_UART_Receive(&this->huart, (uint8_t*)(rxbuf), size, timeout));
	if(this->device)
		device->endUartTransfer(this,false);
	return status == HAL_OK;
}

bool UARTPort::receive_DMA(char* rxbuf,uint16_t size){
	if(this->device)
		device->startUartTransfer(this,false);
	rxbuf_t = rxbuf;
	return(HAL_UART_Receive_DMA(&this->huart, (uint8_t*)(rxbuf), size) == HAL_OK);
}

bool UARTPort::receive_IT(char* rxbuf,uint16_t size){
	if(this->device)
		device->startUartTransfer(this,false);
	rxbuf_t = rxbuf;
	return(HAL_UART_Receive_IT(&this->huart, (uint8_t*)(rxbuf), size) == HAL_OK);
}

bool UARTPort::reservePort(UARTDevice* device){
	if(this->device == nullptr || device == this->device){
		this->device = device;
		return true;
	}
	return false;
}

bool UARTPort::abortReceive(){

	uint32_t status = HAL_UART_AbortReceive(&this->huart);
	if(device && status == HAL_OK){
		device->endUartTransfer(this,false);
	}
	return status == HAL_OK;
}

UART_HandleTypeDef* UARTPort::getHuart(){
	return &this->huart;
}

bool UARTPort::abortTransmit(){
	uint32_t status = HAL_UART_AbortTransmit(&this->huart);
	if(device && status == HAL_OK){
		device->endUartTransfer(this,true);
	}
	return status == HAL_OK;
}

bool UARTPort::freePort(UARTDevice* device){
	if(device == this->device){
		this->device = nullptr;
		return true;
	}
	return false;
}

bool UARTPort::takeSemaphore(bool txsem,uint32_t blocktime){
	cpp_freertos::BinarySemaphore& sem = !txsem ? rxsemaphore : semaphore;
	bool isIsr = inIsr();
	BaseType_t taskWoken = 0;
	bool success = false;
	if(isIsr)
		success = sem.TakeFromISR(&taskWoken);
	else
		success = sem.Take(blocktime);
	isTakenFlag = true;
	portYIELD_FROM_ISR(taskWoken);
	return success;
}

bool UARTPort::giveSemaphore(bool txsem){
	cpp_freertos::BinarySemaphore& sem = !txsem ? rxsemaphore : semaphore;
	bool isIsr = inIsr();
	BaseType_t taskWoken = 0;
	bool success = false;
	if(isIsr)
		success = sem.GiveFromISR(&taskWoken);
	else
		success = sem.Give();
	isTakenFlag = false;
	portYIELD_FROM_ISR(taskWoken);
	return success;
}

bool UARTPort::isTaken(){
	return isTakenFlag;
}

uint32_t UARTPort::getErrors(){
	return huart.ErrorCode;
}


void UARTPort::uartRxComplete(UART_HandleTypeDef *huart){
	// Check if port matches this port
	if(huart == &this->huart){
		if(this->device != nullptr){
			waitingForSingleBytes = false;
			if(isTakenFlag)
				device->endUartTransfer(this,false);
			if(rxbuf_t){
				device->uartRcv(*rxbuf_t);
				rxbuf_t = nullptr;
			}else{
				device->uartRcv((char&)*this->uart_buf);
			}

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
	port->takeSemaphore(transmit);
}

void UARTDevice::endUartTransfer(UARTPort* port,bool transmit){
	port->giveSemaphore(transmit);
}


