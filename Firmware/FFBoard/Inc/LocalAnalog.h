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

struct FFBWheelAnalogConfig{
	uint8_t analogmask = 0xff;
	bool invertX = false;
};

class LocalAnalog : public AnalogSource, public CommandHandler{
public:
	LocalAnalog();
	virtual ~LocalAnalog();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash

	std::vector<int32_t>* getAxes();

	ParseStatus command(ParsedCommand* cmd,std::string* reply);

private:
	static FFBWheelAnalogConfig decodeAnalogConfFromInt(uint16_t val);
	static uint16_t encodeAnalogConfToInt(FFBWheelAnalogConfig conf);

	FFBWheelAnalogConfig aconf;
};



#endif /* SRC_LOCALANALOG_H_ */
