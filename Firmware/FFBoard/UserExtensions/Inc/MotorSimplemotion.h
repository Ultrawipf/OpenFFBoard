/*
 * MotorSimplemotion.h
 *
 *  Created on: Jan 9, 2023
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_MOTORSIMPLEMOTION_H_
#define USEREXTENSIONS_SRC_MOTORSIMPLEMOTION_H_

#include "MotorDriver.h"
#include "cpp_target_config.h"
#include "Encoder.h"
#include "thread.hpp"
#include "CommandHandler.h"
#include "PersistentStorage.h"
#include "UART.h"

#ifdef SIMPLEMOTION

/**
 * Requires a uart port and one GPIO output for the transceiver
 */
class MotorSimplemotion : public MotorDriver,public PersistentStorage, public Encoder,public CommandHandler,public UARTDevice{
public:
	MotorSimplemotion(uint8_t address);
	virtual ~MotorSimplemotion();

	const ClassIdentifier getInfo() = 0;

	void turn(int16_t power) override;
//	void stopMotor() override;
//	void startMotor() override;
	Encoder* getEncoder() override;
	bool hasIntegratedEncoder() {return true;}

	//float getPos_f() override;
	uint32_t getCpr() override;
	int32_t getPos() override;
	void setPos(int32_t pos) override;
	EncoderType getEncoderType() override;

	void saveFlash() override; 		// Write to flash here
	void restoreFlash() override;	// Load from flash

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;
	void registerCommands();
	std::string getHelpstring(){return "RS485 Simplemotion interface";};

	void uartRcv(char& buf); //Warning: called by interrupts!
	void startUartTransfer(UARTPort* port);
	void endUartTransfer(UARTPort* port);

protected:
	const OutputPin& writeEnablePin = gpMotor;
	UARTPort& uart = motor_uart;

	static uint8_t tableCRC8[256];
	static const uint8_t crcpoly = 0x07;
	static bool crcTableInitialized;

private:
	uint8_t address;
};



/**
 * Instance 1
 */
class MotorSimplemotion1 : public MotorSimplemotion{
public:
	MotorSimplemotion1() : MotorSimplemotion{1} {inUse = true;}
	const ClassIdentifier getInfo(){return info;}
	~MotorSimplemotion1(){inUse = false;}
	static bool isCreatable(){return !inUse;}
	static ClassIdentifier info;
private:
	static bool inUse;
};

/**
 * Instance 2
 */
class MotorSimplemotion2 : public MotorSimplemotion{
public:
	MotorSimplemotion2() : MotorSimplemotion{2} {inUse = true;}
	const ClassIdentifier getInfo(){return info;}
	~MotorSimplemotion2(){inUse = false;}
	static bool isCreatable(){return !inUse;}
	static ClassIdentifier info;
private:
	static bool inUse;
};
#endif
#endif /* USEREXTENSIONS_SRC_MOTORSIMPLEMOTION_H_ */
