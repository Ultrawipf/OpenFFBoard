/*
 * TimerHandler.h
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#ifndef TIMERHANDLER_H_
#define TIMERHANDLER_H_

#include "cppmain.h"
#include "global_callbacks.h"

class TimerHandler {
public:
	TimerHandler();
	virtual ~TimerHandler();
	virtual void timerElapsed(TIM_HandleTypeDef* htim);
};

#endif /* TIMERHANDLER_H_ */
