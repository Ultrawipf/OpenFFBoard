/*
 * FFBoardMain.h
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#ifndef FFBOARDMAIN_H_
#define FFBOARDMAIN_H_

#include "main.h"
#include <string>
#include "cmdparser.h"
#include "usbd_cdc_if.h"

struct FFBoardMainIdentifier {
	const char* name;
	uint16_t id;
	bool hidden = false;
};



class FFBoardMain {
public:
	static FFBoardMainIdentifier info;
	virtual const FFBoardMainIdentifier getInfo();

	FFBoardMain();
	virtual ~FFBoardMain();

	virtual void usbInit(); // initialize a composite usb device

	// Callbacks
	virtual void update();
	virtual void cdcRcv(char* Buf, uint32_t *Len);
	virtual void uartRcv(UART_HandleTypeDef *huart);
	virtual void SOF();
	virtual void adcUpd(volatile uint32_t* ADC_BUF);
	virtual void exti(uint16_t GPIO_Pin); // External Interrupt
	virtual void timerElapsed(TIM_HandleTypeDef* htim);

private:

protected:
	virtual void executeCommands(std::vector<ParsedCommand> commands);
	virtual bool executeUserCommand(ParsedCommand* cmd,std::string* reply); // Append reply strings to reply buffer
	virtual bool executeSysCommand(ParsedCommand* cmd,std::string* reply);
	cmdparser parser = cmdparser();

};

#endif /* FFBOARDMAIN_H_ */
