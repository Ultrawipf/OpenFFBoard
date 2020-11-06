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

class AnalogSource : virtual ChoosableClass {
public:
	AnalogSource();
	virtual ~AnalogSource();

	const virtual ClassIdentifier getInfo() = 0;
	static ClassIdentifier info;

	std::vector<uint32_t>* getAxes();

private:
	std::vector<uint32_t> buf;
};

#endif /* SRC_ANALOGSOURCE_H_ */
