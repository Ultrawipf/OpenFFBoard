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
class MotorSimplemotion : public MotorDriver, public Encoder,public CommandHandler,public UARTDevice{

	enum class MotorSimplemotion_commands : uint8_t {
		crcerrors,uarterrors,voltage,torque,status,restart,reg,devtype
	};

	enum class MotorSimplemotion_cmdtypes : uint8_t {
		param32b = 0,param24b = 1,setparamadr = 2,status=3,none = 0xff
	};

	enum class MotorSimplemotion_param : uint16_t {
		FBD = 493, FBR = 565,ControlMode = 559,Voltage = 900, Torque = 901,systemcontrol = 554,status = 553,CB1 = 2533,cumstat = 13,faults = 552,devtype = 6020
	};

	enum class MotorSimplemotion_FBR : uint8_t {
		none = 0,ABN1,ABN2,Resolver,Hall,Serial,Sincos16,Sincos64,Sincos256
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

public:
	MotorSimplemotion(uint8_t instance);
	virtual ~MotorSimplemotion();

	const ClassIdentifier getInfo() = 0;

	void turn(int16_t power) override;
	void stopMotor() override;
	void startMotor() override;
	Encoder* getEncoder() override;
	bool hasIntegratedEncoder() {return true;}


	int32_t getPos() override;
	void setPos(int32_t pos) override;
	EncoderType getEncoderType() override;
	uint32_t getCpr() override;

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;
	void registerCommands();
	std::string getHelpstring(){return "RS485 Simplemotion interface";};

	void uartRcv(char& buf); //Warning: called by interrupts!
	void sendFastUpdate(uint16_t val1,uint16_t val2 = 0);
	void startUartTransfer(UARTPort* port,bool transmit);
	void endUartTransfer(UARTPort* port,bool transmit);
	bool sendCommand(uint8_t* buf,uint8_t len,uint8_t adr);

	uint8_t queueCommand(uint8_t* buf, MotorSimplemotion_cmdtypes type,uint32_t data);
	bool read1Parameter(MotorSimplemotion_param paramId,uint32_t* reply_p,MotorSimplemotion_cmdtypes replylen = MotorSimplemotion_cmdtypes::none); // Check default param length
	bool set1Parameter(MotorSimplemotion_param paramId,int32_t value,uint32_t* reply_p = nullptr);
	bool getSettings();

	uint32_t getCumstat();

	bool motorReady();

	int16_t getTorque();
	int16_t getVoltage();

	void restart();

	static const uint8_t SMCMD_FAST_UPDATE_CYCLE = 2<<3;
	static const uint8_t SMCMD_FAST_UPDATE_CYCLE_RET = (2<<3) | 1;
	static const uint8_t SMP_FAST_UPDATE_CYCLE_FORMAT = 17;
	static const uint8_t SMCMD_INSTANT_CMD = 0x24; // 0x24
	static const uint8_t SMCMD_INSTANT_CMD_RET = 0x25; // 0x25
	static const uint8_t SMPCMD_SETPARAMADDR = 2;

protected:
	const OutputPin& writeEnablePin = gpMotor;

	//CRC
	static std::array<uint8_t,256> tableCRC8;
	static std::array<uint16_t,256> tableCRC16;
	static const uint8_t crcpoly = 0x07;
	static const uint16_t crcpoly16 = 0x8005;
	static bool crcTableInitialized;
	static const uint8_t crc8init = 0x52;

	// Receive buffer
	static const uint8_t RXBUF_SIZE = 32;
	volatile char rxbuf[RXBUF_SIZE];
	volatile uint8_t rxbuf_i = 0;

	// Transmit buffer
	static const uint8_t TXBUF_SIZE = 32;
	char txbuf[TXBUF_SIZE];

	volatile uint8_t replyidx = 0;
	static const uint8_t REPLYBUF_SIZE = 32;
	volatile uint32_t replyvalues[REPLYBUF_SIZE]; // can't be vector because filled in isr

	volatile uint32_t crcerrors = 0;
	int16_t lastTorque = 0;
	volatile uint32_t lastUpdateTime = 0;
	volatile uint32_t uarterrors = 0;

private:
	uint8_t address;
	void updatePosition(uint16_t value);
	void updateStatus(uint16_t value);

	uint16_t status;
	uint16_t devicetype=0;

	int32_t position = 0;
	int32_t position_offset = 0;
	uint16_t lastPosRep = 0x7fff;
	volatile bool waitingFastUpdate = false;
	volatile bool waitingReply = false;
	int32_t overflows = 0;
	Sm2FastUpdate fastbuffer;
	volatile uint32_t lastSentTime = 0;
	volatile uint32_t lastStatusTime = 0;

	static const uint32_t uartErrorTimeout = 10; //ms after a failed transfer to reset the buffer and port status
	volatile uint32_t lastTimeByteReceived = 0;
	volatile bool uartErrorOccured = false;
	void resetBuffer();

	bool prepareUartTransmit();

	bool initialized = false;
	bool hardfault = false;
	Error configError = {ErrorCode::externalConfigurationError, ErrorType::critical, "Simplemotion device invalid configuration"};
	MotorSimplemotion_FBR encodertype;

	static uint16_t calculateCrc16rev(std::array<uint16_t,256> &crctable,uint8_t* buf,uint16_t len,uint16_t crc);

	/**
	 * Templated function to read multiple parameters
	 * TODO: support more than 1 request at once
	 */
	template<size_t params,size_t replynum>
	bool readParameter(std::array<MotorSimplemotion_param,params> paramIds,std::array<uint32_t*,replynum> replies,MotorSimplemotion_cmdtypes replylen = MotorSimplemotion_cmdtypes::none,uint32_t timeout_ms = uartErrorTimeout){
		bool lengthSpecified = replylen != MotorSimplemotion_cmdtypes::none;
		uint8_t subpacketbuf[lengthSpecified ? (4-((uint8_t)replylen & 0x3)+7) * params : 5*params] = {0};
		uint8_t requestlen = 0;

		for(MotorSimplemotion_param param : paramIds){
			if(lengthSpecified){
				requestlen+=queueCommand(subpacketbuf+requestlen,  MotorSimplemotion_cmdtypes::setparamadr, 10); //SMP_RETURN_PARAM_LEN
				requestlen+=queueCommand(subpacketbuf+requestlen,  MotorSimplemotion_cmdtypes::param24b, (uint32_t)replylen); //SMPRET_32B. MUST be 24b at least once!!!
			}
			requestlen+=queueCommand(subpacketbuf+requestlen,  MotorSimplemotion_cmdtypes::setparamadr, 9); //SMP_RETURN_PARAM_ADDR
			requestlen+=queueCommand(subpacketbuf+requestlen,  MotorSimplemotion_cmdtypes::param24b, (uint32_t)param);
		}

		if(!sendCommand(subpacketbuf,requestlen,this->address)){
			uartErrorOccured = true;
			return false;
		}
		uint32_t mstart = HAL_GetTick();

		while((HAL_GetTick()-mstart) < timeout_ms && waitingReply){
			// Wait...
			refreshWatchdog();
		}

		if(waitingReply){
			uartErrorOccured = true;
			uarterrors++;
			//uartport->abortReceive();
			// resetBuffer();

			waitingReply = false;
			return false;
		}
		for(uint8_t i = 0;i<replynum ; i++){
			if(lengthSpecified){
				*replies[i] = replyvalues[(i+1)*3]; // Skip first 3 replies if length is set
			}else{
				*replies[i] = replyvalues[(i+1)*2 - 1]; // Skip first reply
			}
		}

		return true;
	}

	template<size_t params,size_t replynum>
	bool writeParameter(std::array<std::pair<MotorSimplemotion_param,int32_t>,params> paramIds_value,std::array<uint32_t*,replynum> replies,MotorSimplemotion_cmdtypes type,uint32_t timeout_ms = uartErrorTimeout){

		uint8_t packetlen = 4-((uint8_t)type & 0x3);
		uint8_t subpacketbuf[(packetlen + 2)*params] = {0};
		uint8_t requestlen = 0;

		for(std::pair<MotorSimplemotion_param,int32_t> param : paramIds_value){
			requestlen+=queueCommand(subpacketbuf+requestlen,  MotorSimplemotion_cmdtypes::setparamadr, (uint32_t)param.first);
			requestlen+=queueCommand(subpacketbuf+requestlen,  type, (uint32_t)param.second);
		}

		if(!sendCommand(subpacketbuf,requestlen,this->address)){
			return false;
		}
		uint32_t mstart = HAL_GetTick();

		while((HAL_GetTick()-mstart) < timeout_ms && waitingReply){
			// Wait...
			refreshWatchdog();
		}

		if(waitingReply){
			uartErrorOccured = true;
			uarterrors++;
			//uartport->abortReceive();
			waitingReply = false;
			return false;
		}
		for(uint8_t i = 0;i<replynum ; i++){
			if(replies[i])
				*replies[i] = replyvalues[(i+1)*2 - 1]; // Skip first reply
		}

		return true;
	}
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
