/*
 * CommandInterface.cpp
 *
 *  Created on: 17.11.2021
 *      Author: Yannick
 */

#include "CommandInterface.h"
#include "CDCcomm.h"
#include "global_callbacks.h"

std::vector<CommandInterface*> CommandInterface::cmdInterfaces;

CommandInterface::CommandInterface() {
	addCallbackHandler(CommandInterface::cmdInterfaces, this);
}

CommandInterface::~CommandInterface() {
	removeCallbackHandler(CommandInterface::cmdInterfaces, this);
}

/**
 * A batch of commands has been executed and a reply returned
 */
void CommandInterface::commandsDone(std::string* reply,FFBoardMainCommandThread* parser){

}

/**
 * Command thread requests new commands
 * Return true if any are ready, false otherwise
 * If true it will request the vector of parsed commands
 */
bool CommandInterface::hasNewCommands(){
	return parserReady;
}

/*
 *
 * **********************************************************
 *
 */


CDC_CommandInterface::CDC_CommandInterface() {

}

CDC_CommandInterface::~CDC_CommandInterface() {

}


/**
 * Adds a buffer to the parser
 * if it returns true resume the thread
 */
bool CDC_CommandInterface::addBuf(char* Buf, uint32_t *Len){
	bool res = this->parser.add(Buf, Len);
	if(res){
		parserReady = true; // Signals that we should execute commands in the thread
		FFBoardMainCommandThread::wakeUp();
	}
	return res;
}

void CDC_CommandInterface::commandsDone(std::string* reply,FFBoardMainCommandThread* parser){
	//if(parser == this->systemCommands.get()){
	CDCcomm::cdcSend(reply, 0);
	//}
}

std::vector<ParsedCommand> CDC_CommandInterface::getNewCommands(){
	parserReady = false;
	return parser.parse();
}


/*************************
 *
 * Uart command interface
 * Takes bytes from a uart port
 *
 */


extern UARTPort external_uart; // defined in cpp_target_config.cpp
UART_CommandInterface::UART_CommandInterface() : UARTDevice(external_uart){
	uartconfig = uartport->getConfig();
	uartport->registerInterrupt(); // enable port
}

UART_CommandInterface::~UART_CommandInterface() {

}


/*
 * Adds a buffer to the parser
 * if it returns true resume the thread
 */
bool UART_CommandInterface::addBuf(char* Buf, uint32_t *Len){
	bool res = this->parser.add(Buf, Len);
	if(res){
		parserReady = true; // Signals that we should execute commands in the thread
		FFBoardMainCommandThread::wakeUp();
	}
	return res;
}

void UART_CommandInterface::commandsDone(std::string* reply,FFBoardMainCommandThread* parser){
	//if(parser == this->systemCommands.get()){
	uartport->transmit_IT(reply->c_str(), reply->size());
	//}
}

/**
 * Receives one byte and adds it to the parser
 * TODO: seems to cause resets when parsing multiple commands quickly
 */
void UART_CommandInterface::uartRcv(char& buf){
	uint32_t len = 1;
	addBuf(&buf, &len);
}

std::vector<ParsedCommand> UART_CommandInterface::getNewCommands(){
	parserReady = false;
	return parser.parse();
}

