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
#define ODRIVE_THREAD_MEM 256
#define ODRIVE_THREAD_PRIO 25 // Must be higher than main thread

enum class ODriveState : uint32_t {AXIS_STATE_UNDEFINED=0,AXIS_STATE_IDLE=1,AXIS_STATE_STARTUP_SEQUENCE=2,AXIS_STATE_FULL_CALIBRATION_SEQUENCE=3,AXIS_STATE_MOTOR_CALIBRATION=4,AXIS_STATE_ENCODER_INDEX_SEARCH=6,AXIS_STATE_ENCODER_OFFSET_CALIBRATION=7,AXIS_STATE_CLOSED_LOOP_CONTROL=8,AXIS_STATE_LOCKIN_SPIN=9,AXIS_STATE_ENCODER_DIR_FIND=10,AXIS_STATE_HOMING=11,AXIS_STATE_ENCODER_HALL_POLARITY_CALIBRATION=12,AXIS_STATE_ENCODER_HALL_PHASE_CALIBRATION=13};
enum class ODriveControlMode : uint32_t {CONTROL_MODE_VOLTAGE_CONTROL = 0,CONTROL_MODE_TORQUE_CONTROL = 1,CONTROL_MODE_VELOCITY_CONTROL = 2,CONTROL_MODE_POSITION_CONTROL = 3};
enum class ODriveInputMode : uint32_t {INPUT_MODE_INACTIVE = 0,INPUT_MODE_PASSTHROUGH = 1,INPUT_MODE_VEL_RAMP = 2,INPUT_MODE_POS_FILTER = 3,INPUT_MODE_MIX_CHANNELS = 4,INPUT_MODE_TRAP_TRAJ = 5,INPUT_MODE_TORQUE_RAMP =6,INPUT_MODE_MIRROR =7};
enum class ODriveLocalState : uint32_t {IDLE,WAIT_READY,WAIT_CALIBRATION_DONE,WAIT_CALIBRATION,RUNNING,START_RUNNING};

enum class ODriveEncoderFlags : uint32_t {ERROR_NONE = 0,ERROR_UNSTABLE_GAIN = 0x01,ERROR_CPR_POLEPAIRS_MISMATCH = 0x02, ERROR_NO_RESPONSE = 0x04, ERROR_UNSUPPORTED_ENCODER_MODE = 0x08, ERROR_ILLEGAL_HALL_STATE = 0x10, ERROR_INDEX_NOT_FOUND_YET = 0x20, ERROR_ABS_SPI_TIMEOUT = 0x40, ERROR_ABS_SPI_COM_FAIL = 0x80, ERROR_ABS_SPI_NOT_READY = 0x100, ERROR_HALL_NOT_CALIBRATED_YET = 0x200};
enum class ODriveAxisError : uint32_t {AXIS_ERROR_NONE = 0x00000000,AXIS_ERROR_INVALID_STATE  = 0x00000001, AXIS_ERROR_WATCHDOG_TIMER_EXPIRED = 0x00000800,AXIS_ERROR_MIN_ENDSTOP_PRESSED = 0x00001000, AXIS_ERROR_MAX_ENDSTOP_PRESSED = 0x00002000,AXIS_ERROR_ESTOP_REQUESTED = 0x00004000,AXIS_ERROR_HOMING_WITHOUT_ENDSTOP = 0x00020000,AXIS_ERROR_OVER_TEMP = 0x00040000,AXIS_ERROR_UNKNOWN_POSITION = 0x00080000};

enum class ODriveCAN_commands : uint32_t{
	canid,canspd,errors,state,maxtorque,vbus,anticogging,connected,storepos
};

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
		msg.header.rtr = false;
		msg.header.length = sizeof(T);
		msg.header.id = cmd | (nodeId << 5);
		if(!port->sendMessage(msg)){
			// Nothing
		}
	}

	void sendMsg(uint8_t cmd,float value);
	void requestMsg(uint8_t cmd);
	bool motorReady() override;
	void startAnticogging();

	void Run();

	void setTorque(float torque);

	void canRxPendCallback(CANPort* port,CAN_rx_msg& msg) override;
	void canErrorCallback(CANPort* port,uint32_t errcode);


	float getPos_f() override;
	uint32_t getCpr() override;
	int32_t getPos() override;
	void setPos(int32_t pos) override;
	EncoderType getEncoderType() override;

	void setMode(ODriveControlMode controlMode,ODriveInputMode inputMode);
	void setState(ODriveState state);

	void readyCb();

	//void setCanRate(uint8_t canRate);

	void saveFlash() override; 		// Write to flash here
	void restoreFlash() override;	// Load from flash

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;
	void registerCommands();
	std::string getHelpstring(){return "ODrive motor driver with CAN";};

	void setCanFilter();

private:
	CANPort* port = &canport;
	float lastPos = 0;
	float lastSpeed = 0;
	float posOffset = 0;
	float lastVoltage = 0;
	uint32_t lastVoltageUpdate = 0;
	uint32_t lastCanMessage = 0;

	uint32_t lastPosTime = 0;
	bool posWaiting = false;
	bool reloadPosAfterStartup = false;

	int8_t nodeId = 0; // 6 bits can ID
	int8_t motorId = 0;

	volatile ODriveState odriveCurrentState = ODriveState::AXIS_STATE_UNDEFINED;
	volatile uint32_t errors = 0; // Multiple flag bits can be set
	// Not yet used by odrive (0.5.4):
	volatile uint32_t odriveMotorFlags = 0;
	volatile uint32_t odriveEncoderFlags = 0;
	volatile uint32_t odriveControllerFlags = 0;

	float maxTorque = 1.0; // range how to scale the torque output
	volatile bool waitReady = true;

	bool active = false;

	int32_t filterId = 0;
	volatile ODriveLocalState state = ODriveLocalState::IDLE;
	bool connected = false;
};

/**
 * Instance 1 of ODrive
 * Use for M0 output
 */
class ODriveCAN1 : public ODriveCAN{
public:
	ODriveCAN1() : ODriveCAN{0} {inUse = true;}
	const ClassIdentifier getInfo();
	~ODriveCAN1(){inUse = false;}
	static bool isCreatable();
	static ClassIdentifier info;
	static bool inUse;
};

/**
 * Instance 2 of ODrive
 * Use for M1 output
 */
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
