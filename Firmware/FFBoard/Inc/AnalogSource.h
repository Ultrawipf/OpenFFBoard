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
#include "constants.h"


class AnalogSource : public ChoosableClass, public PersistentStorage{
public:

	AnalogSource();
	virtual ~AnalogSource();

	const virtual ClassIdentifier getInfo() = 0;
	static ClassIdentifier info;
	static bool isCreatable() {return true;};
	const ClassType getClassType() override {return ClassType::Analogsource;};

	virtual std::vector<int32_t>* getAxes();
	std::vector<int32_t> buf;

	static const std::vector<class_entry<AnalogSource>> all_analogsources;

private:

};

#endif /* SRC_ANALOGSOURCE_H_ */
