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
	this->restoreFlash();
}

AxesManager::~AxesManager() {
	deleteAxes();
}

void AxesManager::deleteAxes(){
//	for (auto axis : axes) {
//		delete axis;
//	}
	axes.clear();
	axis_count = 0;
}

void AxesManager::setEffectsCalculator(EffectsCalculator* calc) {
	this->effects_calc = calc;
}

void AxesManager::restoreFlash() {
	uint16_t val;
	bool res = (Flash_Read(ADR_AXIS_COUNT, &val));

	if (!res || !this->validAxisRange(val)) {
		val = 1;
	}
	this->setAxisCount(val);
//	for (auto &axis : axes) {
//		axis->restoreFlash();
//	}
}

void AxesManager::saveFlash() {
	Flash_Write(ADR_AXIS_COUNT, this->axis_count);
}

void AxesManager::update() {
	for (auto &axis: axes) {
		axis->prepareForUpdate();
	}
	if (control->usb_update_flag) {
		effects_calc->calculateEffects(axes);
	}
}

void AxesManager::updateTorque() {
	for (auto &axis: axes) {
		axis->updateDriveTorque();
	}
}


std::vector<int32_t>* AxesManager::getAxisValues(){
	for (std::size_t i=0; i < axes.size(); i++) {
		this->axisValues[i] = axes[i]->getLastScaledEnc();
	}
	return &this->axisValues;
}

void AxesManager::emergencyStop() {
	for (auto &axis : axes) {
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
	// Really need to use some form of mutex
	Flash_Write(ADR_AXIS_COUNT, count);

	while (count < axis_count) {
		uint8_t pos = axis_count-1;
		axes.erase(axes.begin()+pos);
		axis_count--;
	}
	while (count > axis_count) {
		axes.push_back( std::make_unique<Axis>('X'+axis_count, control) ); // Axis are indexed from 1-X to 3-Z
		axis_count++;
	}
	control->update_disabled = false;

	// Allocate axis value buffer
	this->axisValues = std::vector<int32_t>(axis_count,0);

	return true;
}

void AxesManager::usbSuspend() {
		for (auto &axis : axes) {
			axis->usbSuspend();
		}
}

void AxesManager::usbResume() {
		for (auto &axis : axes) {
			axis->usbResume();
		}
}

void AxesManager::resetPosZero() {
		for (auto &axis : axes) {
			axis->setPos(0);
		}
}
