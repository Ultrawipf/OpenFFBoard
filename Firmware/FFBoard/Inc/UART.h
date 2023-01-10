/*
 * UART.h
 *
 *  Created on: May 4, 2021
 *      Author: Yannick
 */

#ifndef SRC_UART_H_
#define SRC_UART_H_

#include "semaphore.hpp"
#include "UartHandler.h"
#include "target_constants.h"

/*
 * Example to use the port:
 * port->reservePort(this);
 * //Enables receive interrupt. After its enabled once it will auto reenable after every byte
 * //uartRcv will fire now after every received byte in an interrupt (Don't use usb directly here. just store the byte)
 * port->registerInterrupt();
 * const char x[] = "ABC";
 * port->transmit(x, sizeof(x), 1000); // Transmits a string blocking
 * port->transmit_IT(x, sizeof(x)); // Transmit non blocking
 */
class UARTDevice;
class UARTPort : public UartHandler {
public:
	UARTPort(UART_HandleTypeDef& huart);
	//UARTPort(USART_TypeDef * port);
	virtual ~UARTPort();

	void uartRxComplete(UART_HandleTypeDef *huart);
	void uartTxComplete(UART_HandleTypeDef *huart);

	void transmit(const char* txbuf,uint16_t size,uint32_t timeout = 10000);
	void transmit_IT(const char* txbuf,uint16_t size);
	bool receive(char* rxbuf,uint16_t size,uint32_t timeout = 10000); // Receive in blocking mode
	void transmit_DMA(const char* txbuf,uint16_t size);
	bool receiveDMA(char* rxbuf,uint16_t size);
	bool receiveIT(char* rxbuf,uint16_t size);

	void takeSemaphore(); // Call before accessing this port
	void giveSemaphore(); // Call when finished using this port
	bool isTaken(); // Returns true if semaphore was taken by another task

	bool reconfigurePort(UART_InitTypeDef& config);
	UART_InitTypeDef& getConfig();

	bool isReserved(){return this->device != nullptr;}
	bool reservePort(UARTDevice* device);
	bool freePort(UARTDevice* device);

	void registerInterrupt();

private:
	cpp_freertos::BinarySemaphore semaphore = cpp_freertos::BinarySemaphore(true);
	bool isTakenFlag = false;
	UART_HandleTypeDef& huart;
	UARTDevice* device = nullptr;
	volatile char uart_buf[UART_BUF_SIZE] = {0};
};


class UARTDevice{
public:
	UARTDevice();
	UARTDevice(UARTPort& port);
	virtual ~UARTDevice();
	virtual void uartRcv(char& buf){}; //Warning: called by interrupts!

	virtual void startUartTransfer(UARTPort* port,bool transmit);
	virtual void endUartTransfer(UARTPort* port,bool transmit);

protected:
	UARTPort* uartport = nullptr;
};


#endif /* SRC_UART_H_ */
