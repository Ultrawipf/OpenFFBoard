/*
 * LocalAnalog.h
 *
 *  Created on: Nov 6, 2020
 *      Author: Yannick
 */

#ifndef SRC_LOCALANALOG_H_
#define SRC_LOCALANALOG_H_
#include "AnalogSource.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include <vector>

class LocalAnalog : public AnalogSource {
public:
	LocalAnalog();
	virtual ~LocalAnalog();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	std::vector<uint32_t>* getAxes();

private:
	std::vector<uint32_t> buf;
};



#endif /* SRC_LOCALANALOG_H_ */
