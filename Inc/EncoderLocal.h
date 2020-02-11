/*
 * EncoderLocal.h
 *
 *  Created on: 02.02.2020
 *      Author: Yannick
 */

#ifndef ENCODERLOCAL_H_
#define ENCODERLOCAL_H_

#include "cppmain.h"
#include <Encoder.h>
#include "constants.h"

extern TIM_TypeDef TIM_ENC;

class EncoderLocal: public Encoder {
public:
	EncoderLocal();
	virtual ~EncoderLocal();

	int32_t getPos();
	void setPos(int32_t pos);
	void setOffset(int32_t offset);
	void setPeriod(uint32_t period);
	void overflowCallback();
	//void zpinCallback();

private:
	TIM_TypeDef* htim;
	int32_t offset = 0;
	int32_t pos = 0;
};

#endif /* ENCODERLOCAL_H_ */
