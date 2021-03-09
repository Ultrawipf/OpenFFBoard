/*
 * TimerHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "TimerHandler.h"
#include "global_callbacks.h"

std::vector<TimerHandler*> TimerHandler::timerHandlers;

TimerHandler::TimerHandler() {
	addCallbackHandler(timerHandlers,this);
}

TimerHandler::~TimerHandler() {
	removeCallbackHandler(timerHandlers,this);
}

void TimerHandler::timerElapsed(TIM_HandleTypeDef* htim){

}
