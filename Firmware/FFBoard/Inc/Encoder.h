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

/*
 * Info about used ids:
 * 0: none
 * 1: tmc reserved
 * 2: local/tmc1
 * 3: tmc3
 * 4: mt encoder
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

	virtual EncoderType getType();

	virtual int32_t getPos();
	virtual float getPos_f();

	virtual int32_t getPosAbs();
	virtual float getPosAbs_f();

	virtual void setPos(int32_t pos);

	virtual uint32_t getCpr(); // Encoder counts per rotation


	static const std::vector<class_entry<Encoder> > all_encoders;
	virtual const ClassType getClassType() override {return ClassType::Encoder;};

protected:
	uint32_t cpr = 0;
};


#endif /* ENCODER_H_ */
