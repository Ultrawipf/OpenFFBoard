/*
 * OdriveCAN.h
 *
 *  Created on: Jun 21, 2021
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_SRC_ODRIVECAN_H_
#define USEREXTENSIONS_SRC_ODRIVECAN_H_
#include "MotorDriver.h"
#include "cpp_target_config.h"
#include "CAN.h"
#include "Encoder.h"
#include "thread.hpp"
#include "CanHandler.h"
#include "CommandHandler.h"
#include "PersistentStorage.h"

#ifdef ODRIVE
#define ODRIVE_THREAD_MEM 512
#define ODRIVE_THREAD_PRIO 25 // Must be higher than main thread

enum class ODriveState : uint32_t {AXIS_STATE_UNDEFINED=0,AXIS_STATE_IDLE=1,AXIS_STATE_STARTUP_SEQUENCE=2,AXIS_STATE_FULL_CALIBRATION_SEQUENCE=3,AXIS_STATE_MOTOR_CALIBRATION=4,AXIS_STATE_ENCODER_INDEX_SEARCH=6,AXIS_STATE_ENCODER_OFFSET_CALIBRATION=7,AXIS_STATE_CLOSED_LOOP_CONTROL=8,AXIS_STATE_LOCKIN_SPIN=9,AXIS_STATE_ENCODER_DIR_FIND=10,AXIS_STATE_HOMING=11,AXIS_STATE_ENCODER_HALL_POLARITY_CALIBRATION=12,AXIS_STATE_ENCODER_HALL_PHASE_CALIBRATION=13};
enum class ODriveControlMode : uint32_t {CONTROL_MODE_VOLTAGE_CONTROL = 0,CONTROL_MODE_TORQUE_CONTROL = 1,CONTROL_MODE_VELOCITY_CONTROL = 2,CONTROL_MODE_POSITION_CONTROL = 3};
enum class ODriveInputMode : uint32_t {INPUT_MODE_INACTIVE = 0,INPUT_MODE_PASSTHROUGH = 1,INPUT_MODE_VEL_RAMP = 2,INPUT_MODE_POS_FILTER = 3,INPUT_MODE_MIX_CHANNELS = 4,INPUT_MODE_TRAP_TRAJ = 5,INPUT_MODE_TORQUE_RAMP =6,INPUT_MODE_MIRROR =7};

class ODriveCAN : public MotorDriver,public PersistentStorage, public Encoder, public CanHandler, public CommandHandler, cpp_freertos::Thread{
public:
	ODriveCAN(uint8_t id);
	virtual ~ODriveCAN();


	//static ClassIdentifier info;
	const ClassIdentifier getInfo() = 0;
	//static bool isCreatable();

	void turn(int16_t power) override;
	void stopMotor() override;
	void startMotor() override;
	Encoder* getEncoder() override;
	bool hasIntegratedEncoder() {return true;}

	template<class T>
	void sendMsg(uint8_t cmd,T value){
		CAN_tx_msg msg;
		memcpy(&msg.data,&value,sizeof(T));
		msg.header.RTR = CAN_RTR_DATA;
		msg.header.DLC = sizeof(T);
		msg.header.StdId = cmd | (nodeId << 5);
		port->sendMessage(msg);
	}

	void sendMsg(uint8_t cmd,float value);
	void requestMsg(uint8_t cmd);
	bool motorReady() override;
	void startAnticogging();

	void Run();

	void setTorque(float torque);

	void canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo) override;

	float getPos_f() override;
	uint32_t getCpr() override;
	int32_t getPos() override;
	void setPos(int32_t pos) override;

	void setMode(ODriveControlMode controlMode,ODriveInputMode inputMode);
	void setState(ODriveState state);

	void readyCb();

	void setCanRate(uint8_t canRate);

	void saveFlash() override; 		// Write to flash here
	void restoreFlash() override;	// Load from flash

	ParseStatus command(ParsedCommand* cmd,std::string* reply) override;
	std::string getHelpstring(){return "ODrive: odriveCanId,odriveCanSpd (3=250k,4=500k,5=1M),odriveErrors,odriveState, (Nm*100, scaler),odriveVbus,odriveAnticogging\n";};

private:
	CANPort* port = &canport;
	float lastPos = 0;
	float lastSpeed = 0;
	float posOffset = 0;
	float lastVoltage = 0;
	uint32_t lastVoltageUpdate = 0;

	int8_t nodeId = 0; // 6 bits can ID
	int8_t motorId = 0;

	volatile ODriveState currentState = ODriveState::AXIS_STATE_UNDEFINED;
	volatile uint32_t errors = 0;

	float maxTorque = 1.0; // range how to scale the torque output
	bool ready = false;
	bool waitReady = true;
	volatile bool requestFirstRun = false;
	bool active = false;

	int32_t filterId = 0;

	uint8_t baudrate = CANSPEEDPRESET_500; // 250000, 500000, 1M
};

class ODriveCAN1 : public ODriveCAN{
public:
	ODriveCAN1() : ODriveCAN{0} {inUse = true;}
	const ClassIdentifier getInfo();
	~ODriveCAN1(){inUse = false;}
	static bool isCreatable();
	static ClassIdentifier info;
	static bool inUse;
};

class ODriveCAN2 : public ODriveCAN{
public:
	ODriveCAN2() : ODriveCAN{1} {inUse = true;}
	const ClassIdentifier getInfo();
	~ODriveCAN2(){inUse = false;}
	static bool isCreatable();
	static ClassIdentifier info;
	static bool inUse;
};

#endif /* USEREXTENSIONS_SRC_ODRIVECAN_H_ */
#endif
