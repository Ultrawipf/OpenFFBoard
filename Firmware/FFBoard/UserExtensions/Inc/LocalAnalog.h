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

struct MinMaxPair{
	int32_t min = INT_MAX;
	int32_t max = INT_MIN;
};

struct LocalAnalogConfig{
	uint8_t analogmask = 0xff;
	bool autorange = false;
};

class LocalAnalog : public AnalogSource, public CommandHandler{
	enum class LocalAnaloc_commands : uint32_t{
		pinmask,autocal,pins
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
	bool autorange = false;
	static LocalAnalogConfig decodeAnalogConfFromInt(uint16_t val);
	static uint16_t encodeAnalogConfToInt(LocalAnalogConfig conf);
	const uint8_t numPins = ADC_PINS;
	MinMaxPair minMaxVals[ADC_PINS];


	LocalAnalogConfig aconf;
};



#endif /* SRC_LOCALANALOG_H_ */
