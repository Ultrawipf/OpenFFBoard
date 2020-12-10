/*
 * AnalogSource.h
 *
 *  Created on: 06.11.2020
 *      Author: Yannick
 */

#ifndef SRC_ANALOGSOURCE_H_
#define SRC_ANALOGSOURCE_H_

#include "cppmain.h"
#include "ChoosableClass.h"
#include "PersistentStorage.h"
#include "vector"
#include "CommandHandler.h"

class AnalogSource : virtual ChoosableClass, public PersistentStorage{
public:
	AnalogSource();
	virtual ~AnalogSource();

	const virtual ClassIdentifier getInfo() = 0;
	static ClassIdentifier info;

	virtual std::vector<int32_t>* getAxes();
	std::vector<int32_t> buf;



private:

};

#endif /* SRC_ANALOGSOURCE_H_ */
