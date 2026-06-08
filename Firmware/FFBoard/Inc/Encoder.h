/*
 * Encoder.h
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */

#ifndef ENCODER_H_
#define ENCODER_H_

#include "FFBoardMain.h"
#include "ChoosableClass.h"
#include "KinematicKalman.h"

/*
 * Note:
 * Encoders should count UP when turned counterclockwise
 * This is not the default gamepad direction but matches most motors
 * If direction is not fixed the encoder class should provide a reverse option
 */

/*
 * Info encoder type:
 * Incremental: relative position with no homing
 * Incremental with index: Absolute position available after homing
 * Absolute: Absolute position always available
 */
enum class EncoderType : uint8_t {NONE=0,incremental=1,incrementalIndex=2,absolute=3};

class Encoder : public ChoosableClass {
public:
	Encoder();
	virtual ~Encoder();
	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	virtual EncoderType getEncoderType();

	virtual int32_t getPos() = 0;
	virtual float getPos_f();

	virtual int32_t getPosAbs();
	virtual float getPosAbs_f();

	virtual void setPos(int32_t pos);

	virtual uint32_t getCpr() = 0; // Encoder counts per rotation

	virtual float getSpeed();
	virtual float getAccel();
	virtual uint32_t getScaler();
	virtual void triggerRead();
	void updateState(float new_pos, float dt);

	static const std::vector<class_entry<Encoder> > all_encoders;
	virtual const ClassType getClassType() override {return ClassType::Encoder;};

protected:
	uint32_t cpr = 0;
	KinematicKalman kalman;
	uint32_t last_cpr = 0;
	float r_var = 0.0001f;
	uint32_t last_update_time = 0;
};

class EncoderNone : public Encoder {
public:
	EncoderNone() : Encoder() {}
	~EncoderNone() override = default;
	static ClassIdentifier info;
	const ClassIdentifier getInfo() override { return info; }
	int32_t getPos() override { return 0; }
	uint32_t getCpr() override { return 0; }
	EncoderType getEncoderType() override { return EncoderType::NONE; }
};

#endif /* ENCODER_H_ */
