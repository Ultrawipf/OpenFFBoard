/*
 * ExtiHandler.cpp
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#include "ExtiHandler.h"
#include "global_callbacks.h"
#include "vector"

std::vector<ExtiHandler*> ExtiHandler::extiHandlers;

ExtiHandler::ExtiHandler() {
	addCallbackHandler(extiHandlers,this);
}

ExtiHandler::~ExtiHandler() {
	removeCallbackHandler(extiHandlers, this);
}

void ExtiHandler::exti(uint16_t GPIO_Pin){

}
