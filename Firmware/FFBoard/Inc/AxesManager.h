/*
 * AxesManager.h
 *
 *  Created on: 30.01.2021
 *      Author: Jon Lidgard
 */

#ifndef AXESMANAGER_H_
#define AXESMANAGER_H_
#include "Axis.h"
//#include "NormalizedAxis.h"
#include "PersistentStorage.h"
#include "ffb_defs.h"
#include "hid_cmd_defs.h"

#include "CmdParser.h"
#include "vector"
#include "map"
#include <memory>

class EffectsCalculator;


class AxesManager : public PersistentStorage
{
public:
	AxesManager(volatile Control_t* control);
	virtual ~AxesManager();

//	static ClassIdentifier info;
//	const ClassIdentifier getInfo();

//	ParseStatus command(ParsedCommand *cmd, std::string *reply);
//	void processHidCommand(HID_Custom_Data_t *data);
	virtual std::string getHelpstring() { return "\nAxis commands: (Get: <axis>.<cmd> , Set: <axis>.<cmd>=<val>\nWhere <axis> = x-z e.g. y.power (omitting <axis>. defaults to the X axis.\n"
												 "power,zeroenc,enctype,cpr,pos,degrees,esgain,fxratio,idlespring,friction,invert,drvtype,tmc.\n"; }

	void setEffectsCalculator(EffectsCalculator *ec);
	void saveFlash();
	void restoreFlash();

	uint8_t getAxisCount();
	bool setAxisCount(int8_t count);
	bool validAxisRange(uint8_t val);

	void usbResume();
	void usbSuspend();

	void update();
	void updateTorque();

	std::vector<int32_t>* getAxisValues();

	void emergencyStop();
	void resetPosZero();

private:
	volatile Control_t* control;
	volatile bool *p_usb_disabled;
	volatile bool *p_emergency;
	EffectsCalculator *effects_calc;
	uint16_t axis_count = 0;
	std::vector<std::unique_ptr<Axis>> axes;
	std::vector<int32_t> axisValues = std::vector<int32_t>(1,0);
	//	std::vector<NormalizedAxis*> normalizedAxes;

	void deleteAxes();
};

#endif /* AXESMANAGER_H_ */
