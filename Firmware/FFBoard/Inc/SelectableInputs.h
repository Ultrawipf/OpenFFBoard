/*
 * SelectableInputs.h
 *
 *  Created on: Apr 28, 2025
 *      Author: Yannick
 */

#ifndef SRC_SELECTABLEINPUTS_H_
#define SRC_SELECTABLEINPUTS_H_
#include "AnalogSource.h"
#include "ButtonSource.h"
#include "semaphore.hpp"

/**
 * Helper class with common functions to manage input sources
 */
class SelectableInputs {
public:
	SelectableInputs(const ClassChooser<ButtonSource>& btn_chooser,const ClassChooser<AnalogSource>& analog_chooser);
	virtual ~SelectableInputs();

	virtual void setBtnTypes(uint16_t btntypes);
	virtual void addBtnType(uint16_t id);
	virtual void clearBtnTypes();

	virtual void setAinTypes(uint16_t aintypes);
	virtual void addAinType(uint16_t id);
	virtual void clearAinTypes();

	virtual uint8_t getButtonValues(uint64_t &values);
	virtual std::vector<int32_t>* getAnalogValues();


protected:
	std::vector<std::unique_ptr<ButtonSource>> btns;
	std::vector<std::unique_ptr<AnalogSource>> analog_inputs;

	cpp_freertos::BinarySemaphore sourcesSem = cpp_freertos::BinarySemaphore(true);

	uint16_t btnsources = 0; // Disabled by default
	uint16_t ainsources = 0;

	std::vector<int32_t> analogsources_buf; // Persistent buffer

	ClassChooser<ButtonSource> btn_chooser;
	ClassChooser<AnalogSource> analog_chooser;
};

#endif /* SRC_SELECTABLEINPUTS_H_ */
