/*
 * ExtiHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "ExtiHandler.h"
#include "global_callbacks.h"
#include "vector"

ExtiHandler::ExtiHandler() {
	extern std::vector<ExtiHandler*> extiHandlers;
	addCallbackHandler(&extiHandlers,this);
}

ExtiHandler::~ExtiHandler() {
	extern std::vector<ExtiHandler*> extiHandlers;
	removeCallbackHandler(&extiHandlers, this);
}

void ExtiHandler::exti(uint16_t GPIO_Pin){

}
