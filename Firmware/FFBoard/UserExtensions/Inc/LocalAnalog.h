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

struct MinMaxPair{
	int32_t min = INT_MAX;
	int32_t max = INT_MIN;
};

struct LocalAnalogConfig{
	uint8_t analogmask = 0xff;
	bool autorange = false;
	bool filtersEnabled = false;
};

class LocalAnalog : public AnalogSource, public CommandHandler{
	enum class LocalAnaloc_commands : uint32_t{
		pinmask,autocal,pins,values,filter
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

	void setupFilters();
private:
	bool autorange = false;
	static LocalAnalogConfig decodeAnalogConfFromInt(uint16_t val);
	static uint16_t encodeAnalogConfToInt(LocalAnalogConfig conf);
	const uint8_t numPins = ADC_PINS;
	MinMaxPair minMaxVals[ADC_PINS];

	Biquad filters[ADC_PINS]; // Optional filters
	const float filterF = 30.0/1000.0 , filterQ = 0.5;
	LocalAnalogConfig aconf;
	uint32_t filterSamples = 0;
	const uint32_t waitFilterSamples = 500;
	float autorangeScale = 1.05; // Multiplies autorange scale to add some margin
};



#endif /* SRC_LOCALANALOG_H_ */
