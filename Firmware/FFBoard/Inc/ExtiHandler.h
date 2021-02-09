/*
 * ExtiHandler.h
 *
 *  Created on: Feb 14, 2020
 *      Author: Yannick
 */

#ifndef EXTIHANDLER_H_
#define EXTIHANDLER_H_
#include "cppmain.h"

class ExtiHandler {
public:
	static std::vector<ExtiHandler*> extiHandlers;

	ExtiHandler();
	virtual ~ExtiHandler();
	virtual void exti(uint16_t GPIO_Pin);
};

#endif /* EXTIHANDLER_H_ */
