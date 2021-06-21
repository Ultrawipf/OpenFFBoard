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
#include "CanHandler.h"

#ifdef ODRIVE

enum class OdriveState : uint32_t {AXIS_STATE_UNDEFINED=0,AXIS_STATE_IDLE=1,AXIS_STATE_STARTUP_SEQUENCE=2,AXIS_STATE_FULL_CALIBRATION_SEQUENCE=3,AXIS_STATE_MOTOR_CALIBRATION=4,AXIS_STATE_ENCODER_INDEX_SEARCH=6,AXIS_STATE_ENCODER_OFFSET_CALIBRATION=7,AXIS_STATE_CLOSED_LOOP_CONTROL=8,AXIS_STATE_LOCKIN_SPIN=9,AXIS_STATE_ENCODER_DIR_FIND=10,AXIS_STATE_HOMING=11,AXIS_STATE_ENCODER_HALL_POLARITY_CALIBRATION=12,AXIS_STATE_ENCODER_HALL_PHASE_CALIBRATION=13};
enum class OdriveControlMode : uint32_t {CONTROL_MODE_VOLTAGE_CONTROL = 0,CONTROL_MODE_TORQUE_CONTROL = 1,CONTROL_MODE_VELOCITY_CONTROL = 2,CONTROL_MODE_POSITION_CONTROL = 3};

class OdriveCAN : public MotorDriver, public Encoder, public CanHandler{
public:
	OdriveCAN();
	virtual ~OdriveCAN();


	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	static bool isCreatable();

	void turn(int16_t power) override;
	void stopMotor() override;
	void startMotor() override;
	Encoder* getEncoder();
	bool hasIntegratedEncoder() {return true;}

	void sendMsg(uint8_t cmd,uint64_t value);
	void sendMsg(uint8_t cmd,uint32_t value);
	void sendMsg(uint8_t cmd,float value);
	void requestMsg(uint8_t cmd);
	bool motorReady();

	void setTorque(float torque);

	void canRxPendCallback(CAN_HandleTypeDef *hcan,uint8_t* rxBuf,CAN_RxHeaderTypeDef* rxHeader,uint32_t fifo) override;

	float getPos_f() override;
	uint32_t getCpr() override;
	int32_t getPos() override;

	void setMode(OdriveControlMode mode);

private:
	static bool inUse;
	CANPort* port = &canport;
	float lastPos = 0;
	int8_t shadowCount = 0;
	int8_t nodeId = 0;

	uint8_t status = 0;
	uint8_t errors = 0;

	float torqueConstant = 0.04;
	float maxTorque = 1.0; // range how to scale the torque output
	bool ready = false;

	int32_t filterId = 0;
};

#endif /* USEREXTENSIONS_SRC_ODRIVECAN_H_ */
#endif
