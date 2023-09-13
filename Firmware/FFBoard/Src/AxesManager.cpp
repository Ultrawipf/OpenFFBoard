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

AxesManager::AxesManager(volatile Control_t* control,std::shared_ptr<EffectsCalculator> calc) : control(control), effects_calc(calc) {
	//this->restoreFlash();
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



void AxesManager::update() {
	for (auto &axis: axes) {
		axis->prepareForUpdate();
	}
	if(effects_calc != nullptr)
		effects_calc->calculateEffects(axes);
}

void AxesManager::updateTorque() {
	for (auto &axis: axes) {
		axis->updateDriveTorque();
	}
}


std::vector<int32_t>* AxesManager::getAxisValues(){
	this->axisValues.clear(); // Empty axes
	for (std::size_t i=0; i < axes.size(); i++) {
		Encoder* enc = axes[i]->getEncoder();
		if(enc && enc->getEncoderType() != EncoderType::NONE){ // If encoder not none type
			this->axisValues.push_back(axes[i]->getLastScaledEnc());
			//this->axisValues[i] = axes[i]->getLastScaledEnc();
		}
	}
	return &this->axisValues;
}

void AxesManager::emergencyStop(bool reset) {
	for (auto &axis : axes) {
		axis->emergencyStop(reset);
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
	this->axisValues.reserve(axis_count);
	//this->axisValues = std::vector<int32_t>(axis_count,0);

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
