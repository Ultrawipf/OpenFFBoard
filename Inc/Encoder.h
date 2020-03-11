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

class Encoder : public ChoosableClass {
public:
	Encoder();
	virtual ~Encoder();
	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	virtual int32_t getPos();
	virtual void setPos(int32_t pos);
	virtual uint32_t getPosCpr(); // Position counts per rotation

	virtual uint32_t getPpr(); // Encoder counts per rotation
	virtual void setPpr(uint32_t ppr);	// Encoder counts per rotation
	//virtual void setOffset(int32_t offset);

	//virtual void registerCallback(FFBoardMain);
private:
	int32_t ppr = 0;

};

#endif /* ENCODER_H_ */
