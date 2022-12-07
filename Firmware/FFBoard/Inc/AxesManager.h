/*
 * AxesManager.h
 *
 *  Created on: 30.01.2021
 *      Author: Jon Lidgard
 */

#ifndef AXESMANAGER_H_
#define AXESMANAGER_H_
#include "Axis.h"
//#include "PersistentStorage.h"
#include "ffb_defs.h"

#include "CmdParser.h"
#include "vector"
#include "map"
#include <memory>

class EffectsCalculator;


class AxesManager
{
public:
	AxesManager(volatile Control_t* control,std::shared_ptr<EffectsCalculator> calc);
	virtual ~AxesManager();

//	static ClassIdentifier info;
//	const ClassIdentifier getInfo();

//	void setEffectsCalculator(EffectsCalculator *ec);
//	void saveFlash();
//	void restoreFlash();

	uint8_t getAxisCount();
	bool setAxisCount(int8_t count);
	bool validAxisRange(uint8_t val);

	void usbResume();
	void usbSuspend();

	void update();
	void updateTorque();

	std::vector<int32_t>* getAxisValues();

	void emergencyStop(bool reset);
	void resetPosZero();

private:
	volatile Control_t* control;
	volatile bool *p_usb_disabled;
	volatile bool *p_emergency;
	std::shared_ptr<EffectsCalculator> effects_calc;
	uint16_t axis_count = 0;
	std::vector<std::unique_ptr<Axis>> axes;
	std::vector<int32_t> axisValues = std::vector<int32_t>(1,0);

	void deleteAxes();
};

#endif /* AXESMANAGER_H_ */
