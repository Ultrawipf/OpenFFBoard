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
#include "ExtiHandler.h"
#include "TimerHandler.h"

extern TIM_HandleTypeDef TIM_ENC;

class EncoderLocal: public Encoder,public ExtiHandler,TimerHandler {
public:

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	EncoderLocal();
	virtual ~EncoderLocal();

	EncoderType getType();

	int32_t getPos();
	void setPos(int32_t pos);
	void setOffset(int32_t offset);
	void setPeriod(uint32_t period);
	void overflowCallback();
	void exti(uint16_t GPIO_Pin);
	void timerElapsed(TIM_HandleTypeDef* htim);

private:
	TIM_HandleTypeDef* htim;
	int32_t offset = 0;
	int32_t pos = 0; // Extra position counter for overflows
};

#endif /* ENCODERLOCAL_H_ */
