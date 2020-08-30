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

enum class EncoderType : uint8_t {NONE=0,incremental=1,incrementalIndex=2,absolute=3};

class Encoder : virtual ChoosableClass {
public:
	Encoder();
	virtual ~Encoder();
	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	virtual EncoderType getType();

	virtual int32_t getPos();
	virtual void setPos(int32_t pos);


	virtual uint32_t getCpr(); // Encoder counts per rotation
	virtual void setCpr(uint32_t cpr);	// Encoder counts per rotation

private:
	uint32_t cpr = 0;
};

#endif /* ENCODER_H_ */
