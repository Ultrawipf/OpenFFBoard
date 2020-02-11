/*
 * Encoder.h
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */

#ifndef ENCODER_H_
#define ENCODER_H_

#include "FFBoardMain.h"

class Encoder {
public:
	Encoder();
	virtual ~Encoder();

	virtual int32_t getPos() = 0;
	virtual void setPos(int32_t pos) = 0;
	virtual uint32_t getPosCpr(); // Position counts per rotation

	virtual uint32_t getPpr(); // Encoder counts per rotation
	virtual void setPpr(uint32_t ppr);	// Encoder counts per rotation
	//virtual void setOffset(int32_t offset);

	//virtual void registerCallback(FFBoardMain);
private:
	int32_t ppr = 0;

};

#endif /* ENCODER_H_ */
