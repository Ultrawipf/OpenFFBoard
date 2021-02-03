/*
 * ShifterAnalog.cpp
 *
 *  Created on: 27.02.2020
 *      Author: Yannick
 */

#include <functional>

#include "LocalButtons.h"
#include "ShifterAnalog.h"
#include "global_callbacks.h"
#include "cpp_target_config.h"

ClassIdentifier ShifterAnalog::info = {
		 .name = "Shifter Analog" ,
		 .id=3
 };
const ClassIdentifier ShifterAnalog::getInfo(){
	return info;
}

ShifterAnalog::ShifterAnalog() {
	this->restoreFlash();
}

ShifterAnalog::~ShifterAnalog() {
	if (g27ShifterButtonClient) {
		delete g27ShifterButtonClient;
	}
}

void ShifterAnalog::updateAdc(){
	uint8_t chans = 0;
	volatile uint32_t* buf = getAnalogBuffer(&AIN_HADC,&chans);
	x_val = buf[ADC_CHAN_FPIN+x_chan-1];
	y_val = buf[ADC_CHAN_FPIN+y_chan-1];
}

void ShifterAnalog::calculateGear() {
	gear = 0;

	switch (mode)
	{
	case ShifterMode::G29_seq:
	case ShifterMode::G27_seq:
		// Sequential mode
		if(x_val < X_12){
			gear = 1;
		}else if(x_val > X_56){
			gear = 2;
		}
		break;
	case ShifterMode::G27_H:
	case ShifterMode::G29_H:
		// Calculate h shifter gears by thresholds
		if(x_val < X_12){
		  if(y_val > Y_135)
			  gear=1;       // 1st gear

		  if(y_val < Y_246)
			  gear=2;       // 2nd gear

		}else if(x_val > X_56){
		  if(y_val > Y_135)
			  gear=5;       // 5th gear

		  if(y_val < Y_246)
			  gear=6;       // 6th gear

		}else{
		  if(y_val > Y_135)
			  gear=3;       // 3rd gear

		  if(y_val < Y_246)
			  gear=4;       // 4th gear
		}

		if(gear == 6 && reverseButtonState){
			gear = 7; // Reverse
		}
	}
}

void ShifterAnalog::updateReverseState() {
	if (g27ShifterButtonClient) {
		reverseButtonState = g27ShifterButtonClient->getReverseButton();
	} else {
		reverseButtonState = LocalButtons::readButton(reverseButtonNum-1);
	}
}

int ShifterAnalog::getUserButtons(uint32_t* buf) {
	if (g27ShifterButtonClient) {
		*buf = g27ShifterButtonClient->getUserButtons();
		return g27ShifterButtonClient->numUserButtons;
	}

	return 0;
}

void ShifterAnalog::readButtons(uint32_t* buf){
	updateAdc();

	if (g27ShifterButtonClient) {
		g27ShifterButtonClient->requestUpdate();
	}

	updateReverseState();
	calculateGear();

	*buf = 0;
	// User buttons go first so that switching between sequential and H-pattern
	// doesn't affect user button assignments.
	auto numUserButtons{getUserButtons(buf)};

	if(gear > 0){
		*buf |= 1 << (gear - 1 + numUserButtons);
	}
}

uint16_t ShifterAnalog::getBtnNum(){
	constexpr int numSequentialButtons{2};
	constexpr int numHPatternButtons{7};

	switch(mode) {
		case ShifterMode::G29_seq:
			return numSequentialButtons;
		case ShifterMode::G29_H:
			return numHPatternButtons;
		case ShifterMode::G27_seq:
			return G27ShifterButtonClient::numUserButtons + numSequentialButtons;
		case ShifterMode::G27_H:
			return G27ShifterButtonClient::numUserButtons + numHPatternButtons;
		default:
			return 0;
	}
}

void ShifterAnalog::saveFlash(){
	Flash_Write(ADR_SHIFTERANALOG_CONF, static_cast<uint8_t>(mode));
	Flash_Write(ADR_SHIFTERANALOG_CONF_2, pack(x_chan, y_chan));
	Flash_Write(ADR_SHIFTERANALOG_CONF_3, pack(reverseButtonNum, cs_pin_num));

	Flash_Write(ADR_SHIFTERANALOG_X_12, X_12);
	Flash_Write(ADR_SHIFTERANALOG_X_56, X_56);
	Flash_Write(ADR_SHIFTERANALOG_Y_135, Y_135);
	Flash_Write(ADR_SHIFTERANALOG_Y_246, Y_246);
}

void ShifterAnalog::restoreFlash(){
	std::tie(x_chan, y_chan) = unpack(Flash_Read(ADR_SHIFTERANALOG_CONF_2, pack(x_chan, y_chan)));
	std::tie(reverseButtonNum, cs_pin_num) = unpack(Flash_Read(ADR_SHIFTERANALOG_CONF_3, pack(reverseButtonNum, cs_pin_num)));

	setMode(Flash_Read(ADR_SHIFTERANALOG_CONF, ShifterMode::G29_H));

	X_12 = Flash_Read(ADR_SHIFTERANALOG_X_12, X_12);
	X_56 = Flash_Read(ADR_SHIFTERANALOG_X_56, X_56);
	Y_135 = Flash_Read(ADR_SHIFTERANALOG_Y_135, Y_135);
	Y_246 = Flash_Read(ADR_SHIFTERANALOG_Y_246, Y_246);
}

void ShifterAnalog::printModes(std::string* reply){
	for(uint8_t i = 0; i<mode_names.size();i++){
		*reply+=  mode_names[i] + ":" + std::to_string(i) + "," + std::to_string(mode_uses_spi[i]) + "," + std::to_string(mode_uses_local_reverse[i]) + "\n";
	}
}

bool ShifterAnalog::isG27Mode(ShifterMode m) {
	return m == ShifterMode::G27_H || m == ShifterMode::G27_seq;
}

void ShifterAnalog::setMode(ShifterMode newMode) {
	if (g27ShifterButtonClient && !isG27Mode(newMode)) {
		delete g27ShifterButtonClient;
		g27ShifterButtonClient = nullptr;
	} else if (!g27ShifterButtonClient && isG27Mode(newMode)) {
		g27ShifterButtonClient = new G27ShifterButtonClient(getExternalSPI_CSPin(cs_pin_num));
	}

	mode = newMode;
}

void ShifterAnalog::setCSPin(uint8_t new_cs_pin_num) {
	if (new_cs_pin_num == cs_pin_num) {
		return;
	}

	cs_pin_num = new_cs_pin_num;

	if (g27ShifterButtonClient) {
		g27ShifterButtonClient->updateCSPin(getExternalSPI_CSPin(cs_pin_num));
	}
}



ParseStatus ShifterAnalog::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus result = ParseStatus::OK;
	// use "shifter_mode!" to print a list of possible modes and choose one
	if(cmd->cmd == "shifter_mode"){
		if(cmd->type == CMDtype::set){
			setMode((ShifterMode)cmd->val);
		}else if(cmd->type == CMDtype::get){
			*reply += std::to_string((uint8_t)this->mode);
		}else{
			printModes(reply);
		}
	}else if (cmd->cmd == "shifter_x_12"){
		handleGetSet(cmd, reply, X_12);
	}else if (cmd->cmd == "shifter_x_56"){
		handleGetSet(cmd, reply, X_56);
	}else if(cmd->cmd == "shifter_y_135"){
		handleGetSet(cmd, reply, Y_135);
	}else if(cmd->cmd == "shifter_y_246"){
		handleGetSet(cmd, reply, Y_246);
	}else if (cmd->cmd == "shifter_rev_btn"){
		handleGetSet(cmd, reply, reverseButtonNum);
	}else if (cmd->cmd == "shifter_cs_pin"){
		handleGetSet(cmd, reply, cs_pin_num);
	}else if (cmd->cmd == "shifter_x_chan"){
		handleGetSet(cmd, reply, x_chan);
	}else if (cmd->cmd == "shifter_y_chan"){
		handleGetSet(cmd, reply, y_chan);
	}else if (cmd->cmd == "shifter_vals" && cmd->type == CMDtype::get){
		*reply += std::to_string(x_val) + "," + std::to_string(y_val);
	}else if (cmd->cmd == "shifter_gear" && cmd->type == CMDtype::get){
		*reply += std::to_string(gear);
	}else{
		result = ParseStatus::NOT_FOUND;
	}
	return result;
}

ShifterAnalog::G27ShifterButtonClient::G27ShifterButtonClient(const OutputPin& csPin)
	:config{csPin} {
	config.peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	config.peripheral.FirstBit = SPI_FIRSTBIT_LSB;
	config.peripheral.CLKPhase = SPI_PHASE_1EDGE;
	config.peripheral.CLKPolarity = SPI_POLARITY_LOW;

	attachToPort(external_spi);
}

void ShifterAnalog::G27ShifterButtonClient::updateCSPin(const OutputPin& csPin) {
	config.cs = csPin;
}


bool ShifterAnalog::G27ShifterButtonClient::getReverseButton() {
	return buttonStates & 0x02;
}

uint16_t ShifterAnalog::G27ShifterButtonClient::getUserButtons() {
	return buttonStates >> 4;
}

const SPIConfig& ShifterAnalog::G27ShifterButtonClient::getConfig() const {
	return config;
}

void ShifterAnalog::G27ShifterButtonClient::beginRequest(SPIPort::Pipe& pipe) {
	pipe.beginRx(reinterpret_cast<uint8_t*>(&buttonStates), sizeof(buttonStates));
}
