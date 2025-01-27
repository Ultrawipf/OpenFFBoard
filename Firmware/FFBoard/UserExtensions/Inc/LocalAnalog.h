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
#include <limits.h>
#include "Filters.h"
#include "AnalogAxisProcessing.h"


struct LocalAnalogConfig{
	uint8_t analogmask = 0xff;
};

class LocalAnalog : public AnalogSource, public CommandHandler, public AnalogAxisProcessing{
	enum class LocalAnalog_commands : uint32_t{
		pinmask,pins=2,values=3
	};
public:
	LocalAnalog();
	virtual ~LocalAnalog();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;
	static bool isCreatable() {return true;};

	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash

	std::vector<int32_t>* getAxes();

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void setAutorange(bool autorange);
	virtual std::string getHelpstring(){return "Analog pins source\n";}

	const ClassType getClassType() override {return ClassType::Analogsource;};

private:
	const uint8_t numPins = ADC_PINS;
	uint8_t bitshift = 0;
	uint16_t mask = 0xffff;
	LocalAnalogConfig aconf;
};



#endif /* SRC_LOCALANALOG_H_ */
