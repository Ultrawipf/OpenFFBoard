/*
 * SelectableInputs.cpp
 *
 *  Created on: Apr 28, 2025
 *      Author: Yannick
 */

#include "SelectableInputs.h"

SelectableInputs::SelectableInputs(const ClassChooser<ButtonSource>& btn_chooser,const ClassChooser<AnalogSource>& analog_chooser)
: btn_chooser(btn_chooser), analog_chooser(analog_chooser)
{
	analogsources_buf.reserve(8);
}

SelectableInputs::~SelectableInputs() {
	clearAinTypes();
	clearBtnTypes();
}

// Buttons
void SelectableInputs::clearBtnTypes(){
	// Destruct all button sources

	this->btns.clear();
}

void SelectableInputs::setBtnTypes(uint16_t btntypes){
	sourcesSem.Take();
	this->btnsources = btntypes;
	clearBtnTypes();
	for(uint8_t id = 0;id<16;id++){
		if((btntypes >> id) & 0x1){
			// Matching flag
			ButtonSource* btn = btn_chooser.Create(id);
			if(btn!=nullptr)
				this->btns.push_back(std::unique_ptr<ButtonSource>(btn));
		}
	}
	sourcesSem.Give();
}

void SelectableInputs::addBtnType(uint16_t id){
	for(auto &btn : this->btns){
		if(btn->getInfo().id == id){
			return;
		}
	}
	ButtonSource* btn = btn_chooser.Create(id);
	if(btn!=nullptr)
		this->btns.push_back(std::unique_ptr<ButtonSource>(btn));
}

// Analog
void SelectableInputs::clearAinTypes(){
	// Destruct all button sources

	this->analog_inputs.clear();
}

void SelectableInputs::setAinTypes(uint16_t aintypes){
	sourcesSem.Take();
	this->ainsources = aintypes;
	clearAinTypes();
	for(uint8_t id = 0;id<16;id++){
		if((aintypes >> id) & 0x1){
			// Matching flag
			AnalogSource* ain = analog_chooser.Create(id);
			if(ain!=nullptr)
				this->analog_inputs.push_back(std::unique_ptr<AnalogSource>(ain));
		}
	}
	sourcesSem.Give();
}
void SelectableInputs::addAinType(uint16_t id){
	for(auto &ain : this->analog_inputs){
		if(ain->getInfo().id == id){
			return;
		}
	}
	AnalogSource* ain = analog_chooser.Create(id);
	if(ain!=nullptr)
		this->analog_inputs.push_back(std::unique_ptr<AnalogSource>(ain));
}

/**
 * Gets button values from all button sources
 */
uint8_t SelectableInputs::getButtonValues(uint64_t &values){
	uint8_t shift = 0;
	if(btns.size() != 0){
		for(auto &btn : btns){
			uint64_t buf = 0;
			uint8_t amount = btn->readButtons(&buf);
			values |= buf << shift;
			shift += amount;
		}
	}
	return shift;
}

/**
 * Gets analog values from all analog sources
 */
std::vector<int32_t>* SelectableInputs::getAnalogValues(){
	sourcesSem.Take();
	analogsources_buf.clear();
	for(auto &ain : analog_inputs){
		std::vector<int32_t>* buf = ain->getAxes();
		analogsources_buf.insert(analogsources_buf.end(), buf->begin(), buf->end());
	}
	sourcesSem.Give();
	return &analogsources_buf;
}
