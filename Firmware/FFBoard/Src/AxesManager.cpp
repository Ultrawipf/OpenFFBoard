/*
 * AxesManager.cpp
 *
 *  Created on: 30.01.21
 *      Author: Jon Lidgard
 */

#include <stdint.h>
#include <string>
#include "constants.h"
#include "cmsis_os.h"
#include "AxesManager.h"


//ClassIdentifier AxesManager::info = {
//		.name 	= "NONE" ,
//			 .id	= 0,
//			 .hidden = true
//};
//const ClassIdentifier AxesManager::getInfo(){
//	return info;
//}

AxesManager::AxesManager(volatile Control_t* control) {
	this->control = control;
	uint16_t val;
	Flash_Read(ADR_AXIS_COUNT, &val);
	if (!this->validAxisRange(val)) {
		val = 1;
	}
	this->setAxisCount(val);
}

AxesManager::~AxesManager() {
	deleteAxes();
}

void AxesManager::deleteAxes(){
	for (auto axis : axes) {
		delete axis;
	}
	axes.clear();
	axis_count = 0;
}

void AxesManager::setEffectsCalculator(EffectsCalculator* calc) {
	this->effects_calc = calc;
}

void AxesManager::restoreFlash() {
	for (auto axis : axes) {
		axis->restoreFlash();
	}
}

void AxesManager::saveFlash() {
	Flash_Write(ADR_AXIS_COUNT, this->axis_count);
	for (auto axis : axes) {
		axis->saveFlash();
	}
}

void AxesManager::update() {
	for (auto axis: axes) {
		axis->prepareForUpdate();
	}
	if (control->usb_update_flag) {
		effects_calc->calculateEffects(axes);
	}
}

void AxesManager::updateTorque() {
	for (auto axis: axes) {
		// New torque stored in normalizedAxes.encoderTorque;
		axis->updateDriveTorque();
	}
}

void AxesManager::addAxesToReport(int16_t** report, uint8_t* pCount) {
	for (std::size_t i=0; i < axes.size(); i++) {
			*(report[(*pCount)++]) = (int32_t)axes[i]->getLastScaledEnc();
	}
}

void AxesManager::emergencyStop() {
	for (auto axis : axes) {
		axis->emergencyStop();
	}
}

uint8_t AxesManager::getAxisCount() { return axis_count; }

bool AxesManager::validAxisRange(uint8_t val) {
	return ((val>0) && (val<=MAX_AXIS));
}

bool AxesManager::setAxisCount(int8_t count) {
	if (!this->validAxisRange(count)) {
		return false; // invalid number of axis
	}
	// It'll probably crash if we reduce axis but at least will work on reboot
	// Really need to use some form of mutex
	Flash_Write(ADR_AXIS_COUNT, count);

//	control->request_update_disabled = true;
//	while(control->update_disabled == false) {
//		osDelay(100);
//	}

	while (count < axis_count) {
		delete (Axis*)axes[axis_count-1];
		axis_count--;
	}
	while (count > axis_count) {
		Axis* axis = new Axis('X'+axis_count, control);
		axes.push_back( axis ); // Axis are indexed from 1-X to 3-Z
		axis_count++;
	}
	control->update_disabled = false;
	return true;
}

void AxesManager::usbSuspend() {
		for (auto axis : axes) {
			axis->usbSuspend();
		}
}

void AxesManager::usbResume() {
		for (auto axis : axes) {
			axis->usbResume();
		}
}

void AxesManager::resetPosZero() {
		for (auto axis : axes) {
			axis->setPos(0);
		}
}


// ---- AXis Commands ----

//ParseStatus AxesManager::command(ParsedCommand* cmd,std::string* reply){
//	if (cmd->axis >= 0 && cmd->axis < axes.size()) {
//		return axes[cmd->axis]->command(cmd, reply);
//	}
//	return ParseStatus::NOT_FOUND;
//}

//void AxesManager::processHidCommand(HID_Custom_Data_t *data){
//	uint8_t axis = (data->cmd >> 6) & 0x3;
//	if(axis<axis_count) {
//		return axes[axis]->processHidCommand(data);
//	}else{
//		return false;
//	}
//}
