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

	struct Sm2FastUpdate{
		uint8_t header = SMCMD_FAST_UPDATE_CYCLE;
		uint8_t adr = 1;
		uint16_t val1 = 0;
		uint16_t val2 = 0;
		uint8_t crc = 0;
	} __attribute__((packed));

	struct Sm2FastUpdate_reply{
		uint8_t header = SMCMD_FAST_UPDATE_CYCLE;
		uint16_t val1 = 0;
		uint16_t val2 = 0;
		uint8_t crc = 0;
	} __attribute__((packed));

	struct SMPayloadCommand16{
		long param :14;
		long ID:2; // = 2
	} __attribute__((packed));

	struct SMPayloadCommand24{
		long param :22;
		long ID:2; // = 1
	} __attribute__((packed));

	struct SMPayloadCommand32{
		long param :30;
		long ID:2; // = 0
	} __attribute__((packed));


public:
	MotorSimplemotion(uint8_t instance);
	virtual ~MotorSimplemotion();

	const ClassIdentifier getInfo() = 0;

	void turn(int16_t power) override;
	void stopMotor() override;
	void startMotor() override;
	Encoder* getEncoder() override;
	bool hasIntegratedEncoder() {return true;}

	//float getPos_f() override;
//	uint32_t getCpr() override;
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
	void sendCommand(uint8_t len,uint8_t* buf);

	static const uint8_t SMCMD_FAST_UPDATE_CYCLE = 2<<3;
	static const uint8_t SMCMD_FAST_UPDATE_CYCLE_RET = (2<<3) | 1;
	static const uint8_t SMP_FAST_UPDATE_CYCLE_FORMAT = 17;
	static const uint8_t SMCMD_INSTANT_CMD = (((4)<<3)|4);
	static const uint8_t SMCMD_INSTANT_CMD_RET = (((4)<<3)| 4 | 1);
	static const uint8_t SMPCMD_SETPARAMADDR = 2;

protected:
	const OutputPin& writeEnablePin = gpMotor;

	//CRC
	static std::array<uint8_t,256> tableCRC8;
	static std::array<uint16_t,256> tableCRC16;
	static const uint8_t crcpoly = 0x07;
	static const uint16_t crcpoly16 = 0xC1C0;
	static bool crcTableInitialized;
	static const uint8_t crc8init = 0x52;

	// Receive buffer
	static const uint8_t RXBUF_SIZE = 32;
	volatile char rxbuf[RXBUF_SIZE];
	volatile uint8_t rxbuf_i = 0;

	volatile uint32_t crcerrors = 0;
	int16_t lastTorque = 0;
	volatile uint32_t lastUpdateTime = 0;

private:
	uint8_t address;
	void updatePosition(uint16_t value);
	void updateStatus(uint16_t value);
	int32_t position = 0;
	uint16_t lastPosRep = 0;
	volatile bool waitingFastUpdate = false;
	int32_t overflows = 0;
	Sm2FastUpdate fastbuffer;
	volatile uint32_t lastSentTime = 0;
	void resetBuffer();
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
