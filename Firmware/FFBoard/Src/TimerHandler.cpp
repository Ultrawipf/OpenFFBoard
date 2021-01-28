/*
 * TimerHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "TimerHandler.h"
#include "global_callbacks.h"

TimerHandler::TimerHandler() {
	extern std::vector<TimerHandler*> timerHandlers;
	addCallbackHandler(&timerHandlers,this);

}

TimerHandler::~TimerHandler() {
	extern std::vector<TimerHandler*> timerHandlers;
	removeCallbackHandler(&timerHandlers,this);
}

void TimerHandler::timerElapsed(TIM_HandleTypeDef* htim){

}
