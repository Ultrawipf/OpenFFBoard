/*
 * TimerHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "TimerHandler.h"

TimerHandler::TimerHandler() {
	extern std::vector<TimerHandler*> timerHandlers;
	timerHandlers.push_back(this);

}

TimerHandler::~TimerHandler() {
	extern std::vector<TimerHandler*> timerHandlers;
	for (uint8_t i = 0; i < timerHandlers.size(); i++){
		if(timerHandlers[i] == this){
			timerHandlers.erase(timerHandlers.begin()+i);
			break;
		}
	}
}

void TimerHandler::timerElapsed(TIM_HandleTypeDef* htim){

}
