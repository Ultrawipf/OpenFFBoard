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

	enum class MotorSimplemotion_commands : uint8_t {
		errors
	};
public:
	MotorSimplemotion(uint8_t instance);
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
	void sendFastUpdate(uint16_t val1,uint16_t val2 = 0);
	void startUartTransfer(UARTPort* port,bool transmit);
	void endUartTransfer(UARTPort* port,bool transmit);

	static const uint8_t SMCMD_FAST_UPDATE_CYCLE = 2<<3;
	static const uint8_t SMCMD_FAST_UPDATE_CYCLE_RET = (2<<3) | 1;
	static const uint8_t SMP_FAST_UPDATE_CYCLE_FORMAT = 17;

protected:
	const OutputPin& writeEnablePin = gpMotor;
	UARTPort& uart = motor_uart;

	static std::array<uint8_t,256> tableCRC8;
	static std::array<uint16_t,256> tableCRC16;
	static const uint8_t crcpoly = 0x07;
	static const uint16_t crcpoly16 = 0xC1C0;
	static bool crcTableInitialized;
	uint8_t rxbuf[6];

private:
	uint8_t address;
};



/**
 * Instance 1
 */
class MotorSimplemotion1 : public MotorSimplemotion{
private:
	static bool inUse;
public:
	MotorSimplemotion1() : MotorSimplemotion{0} {inUse = true;}
	static ClassIdentifier info;
	const ClassIdentifier getInfo(){return info;}
	~MotorSimplemotion1(){inUse = false;}
	static bool isCreatable(){return !inUse;}

};

/**
 * Instance 2
 */
class MotorSimplemotion2 : public MotorSimplemotion{
private:
	static bool inUse;
public:
	MotorSimplemotion2() : MotorSimplemotion{1} {inUse = true;}
	static ClassIdentifier info;
	const ClassIdentifier getInfo(){return info;}
	~MotorSimplemotion2(){inUse = false;}
	static bool isCreatable(){return !inUse;}

};
#endif
#endif /* USEREXTENSIONS_SRC_MOTORSIMPLEMOTION_H_ */
