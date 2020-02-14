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
	extiHandlers.push_back(this);
}

ExtiHandler::~ExtiHandler() {
	extern std::vector<ExtiHandler*> extiHandlers;
	for (uint8_t i = 0; i < extiHandlers.size(); i++){
		if(extiHandlers[i] == this){
			extiHandlers.erase(extiHandlers.begin()+i);
			break;
		}
	}
}

void ExtiHandler::exti(uint16_t GPIO_Pin){

}
