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
#include "AdcHandler.h"

ClassIdentifier ShifterAnalog::info = {
		 .name = "Shifter Analog" ,
		 .id=CLSID_BTN_SHIFTER, // 3
 };
const ClassIdentifier ShifterAnalog::getInfo(){
	return info;
}

ShifterAnalog::ShifterAnalog() : CommandHandler("shifter", CLSID_BTN_SHIFTER) {
	uint8_t bits = AdcHandler::getAdcResolutionBits(&AIN_HADC);
	bitshift = std::max(0,16-bits);

	this->restoreFlash();
	this->registerCommands();
}

ShifterAnalog::~ShifterAnalog() {

}

void ShifterAnalog::registerCommands(){
	CommandHandler::registerCommands();

	registerCommand("mode", ShifterAnalog_commands::mode, "Shifter mode",CMDFLAG_GET|CMDFLAG_SET|CMDFLAG_INFOSTRING);
	registerCommand("x12", ShifterAnalog_commands::x12, "X-threshold for 1&2 gears",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("x56", ShifterAnalog_commands::x56, "X-threshold for 5&6 gears",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("y135", ShifterAnalog_commands::y135, "Y-threshold for 1&3&5 gears",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("y246", ShifterAnalog_commands::y246, "Y-threshold for 2&4&6 gears",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("revbtn", ShifterAnalog_commands::revbtn, "Pin for R signal",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("cspin", ShifterAnalog_commands::cspin, "CS pin for SPI modes",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("xchan", ShifterAnalog_commands::xchan, "X signal analog pin",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("ychan", ShifterAnalog_commands::ychan, "Y signal analog pin",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("vals", ShifterAnalog_commands::vals, "Analog values",CMDFLAG_GET);
	registerCommand("gear", ShifterAnalog_commands::gear, "Decoded gear",CMDFLAG_GET);
}

void ShifterAnalog::updateAdc(){
	uint8_t chans = 0;
	volatile uint32_t* buf = getAnalogBuffer(&AIN_HADC,&chans);
	x_val = buf[ADC_CHAN_FPIN+x_chan-1] << bitshift; // Make 16 bit
	y_val = buf[ADC_CHAN_FPIN+y_chan-1] << bitshift;
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

int ShifterAnalog::getUserButtons(uint64_t* buf) {
	if (g27ShifterButtonClient) {
		*buf = g27ShifterButtonClient->getUserButtons();
		return g27ShifterButtonClient->numUserButtons;
	}

	return 0;
}

uint8_t ShifterAnalog::readButtons(uint64_t* buf){
	updateAdc();

	if (g27ShifterButtonClient) {
		// Synchronous read - waits for bus and reads immediately
		g27ShifterButtonClient->startRead();

		// Now process the freshly read data
		updateReverseState();
		getUserButtons(buf);
	} else {
		updateReverseState();
		*buf = 0;
	}

	calculateGear();

	// User buttons go first so that switching between sequential and H-pattern
	// doesn't affect user button assignments.
	if(gear > 0){
		uint64_t numUserButtons = g27ShifterButtonClient ? g27ShifterButtonClient->numUserButtons : 0;
		*buf |= 1 << (gear - 1 + numUserButtons);
	}

	return this->btnnum;
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
	std::tie(x_chan, y_chan) = unpack(Flash_ReadDefault(ADR_SHIFTERANALOG_CONF_2, pack(x_chan, y_chan)));
	std::tie(reverseButtonNum, cs_pin_num) = unpack(Flash_ReadDefault(ADR_SHIFTERANALOG_CONF_3, pack(reverseButtonNum, cs_pin_num)));

	setMode(Flash_ReadDefault(ADR_SHIFTERANALOG_CONF, ShifterMode::G29_H));

	X_12 = Flash_ReadDefault(ADR_SHIFTERANALOG_X_12, X_12);
	X_56 = Flash_ReadDefault(ADR_SHIFTERANALOG_X_56, X_56);
	Y_135 = Flash_ReadDefault(ADR_SHIFTERANALOG_Y_135, Y_135);
	Y_246 = Flash_ReadDefault(ADR_SHIFTERANALOG_Y_246, Y_246);
}

std::string ShifterAnalog::printModes(){
	std::string reply;
	for(uint8_t i = 0; i<mode_names.size();i++){
		reply+=  mode_names[i] + ":" + std::to_string(i) + "," + std::to_string(mode_uses_spi[i]) + "," + std::to_string(mode_uses_local_reverse[i]) + "\n";
	}
	return reply;
}

bool ShifterAnalog::isG27Mode(ShifterMode m) {
	return m == ShifterMode::G27_H || m == ShifterMode::G27_seq;
}

void ShifterAnalog::setMode(ShifterMode newMode) {
	if (g27ShifterButtonClient && !isG27Mode(newMode)) {
		g27ShifterButtonClient.reset();
		g27ShifterButtonClient = nullptr;
	} else if (!g27ShifterButtonClient && isG27Mode(newMode)) {
		// Use the configured CS pin (cs_pin_num is 1-indexed, getCsPin uses 0-indexed)
		OutputPin* csPin = external_spi.getCsPin(cs_pin_num - 1);
		if (csPin == nullptr) {
			// Fallback to first free pin if configured pin is invalid
			auto& freePins = external_spi.getFreeCsPins();
			if (!freePins.empty()) {
				csPin = &freePins[0];
			}
		}
		if (csPin != nullptr) {
			g27ShifterButtonClient = std::make_unique<G27ShifterButtonClient>(*csPin);
			// Initial read will happen on first readButtons() call
		}
	}

	mode = newMode;
	this->btnnum = getBtnNum(); // Update amount
}

void ShifterAnalog::setCSPin(uint8_t new_cs_pin_num) {
	if (new_cs_pin_num == cs_pin_num) {
		return;
	}

	cs_pin_num = new_cs_pin_num;

	if (g27ShifterButtonClient) {
		// cs_pin_num is 1-indexed, getCsPin uses 0-indexed
		OutputPin* newPin = external_spi.getCsPin(cs_pin_num - 1);
		if (newPin != nullptr) {
			g27ShifterButtonClient->updateCSPin(*newPin);
		}
	}
}



CommandStatus ShifterAnalog::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<ShifterAnalog_commands>(cmd.cmdId)){
	case ShifterAnalog_commands::mode:
		if(cmd.type == CMDtype::set){
			setMode((ShifterMode)cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->mode);
		}else if(cmd.type == CMDtype::info){
			replies.emplace_back(printModes());
		}
		break;

	case ShifterAnalog_commands::x12:
		return handleGetSet(cmd, replies, X_12);
	case ShifterAnalog_commands::x56:
		return handleGetSet(cmd, replies, X_56);
	case ShifterAnalog_commands::y135:
		return handleGetSet(cmd, replies, Y_135);
	case ShifterAnalog_commands::y246:
		return handleGetSet(cmd, replies, Y_246);
	case ShifterAnalog_commands::revbtn:
		return handleGetSet(cmd, replies, reverseButtonNum);
	case ShifterAnalog_commands::cspin:
		if(cmd.type == CMDtype::set){
			setCSPin(cmd.val);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(cs_pin_num);
		}else{
			return CommandStatus::ERR;
		}
		break;
	case ShifterAnalog_commands::xchan:
		return handleGetSet(cmd, replies, x_chan);
	case ShifterAnalog_commands::ychan:
		return handleGetSet(cmd, replies, y_chan);
	case ShifterAnalog_commands::vals:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(x_val,y_val);
		}else{
			return CommandStatus::ERR;
		}
		break;
	case ShifterAnalog_commands::gear:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(gear);
		}else{
			return CommandStatus::ERR;
		}
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;
}

ShifterAnalog::G27ShifterButtonClient::G27ShifterButtonClient(OutputPin& csPin)
	:SPIDevice(external_spi,csPin) {
	// Exact same configuration as SPI Buttons in 74xx165 mode
	// Use prescaler 64 (slow speed) - most reliable for shared bus
	spiConfig.peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_LSB;
	// 74HC165 configuration: sample on falling edge (phase 2, polarity high)
	// The 165 shifts on rising edge but has first output set before clocking.
	// First clock cycle is actually second bit, so we sample at falling edge.
	spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
	spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	// CS polarity LOW (active low) for 74HC165 mode (same as SPI Buttons)
	spiConfig.cspol = false;

	// Don't configure SPI port here - let receive_DMA handle it automatically
	// This prevents conflicts when multiple devices share the SPI bus
	// The receive_DMA will reconfigure if needed (when allowReconfigure is true)
}

void ShifterAnalog::G27ShifterButtonClient::startRead() {
	// Use synchronous read - waits for semaphore if SPI_Buttons is using the bus
	// This ensures shifter always gets fresh data
	// The wait is very short since DMA completes in microseconds
	// Both devices now use same SPI config (prescaler 32, mode 74xx165) so no reconfiguration needed
	external_spi.receive(spi_buf, bytesToRead, this, 2); // 2ms timeout
}

bool ShifterAnalog::G27ShifterButtonClient::getReverseButton() {
	// Read directly from spi_buf (DMA writes directly here, like SPI_Buttons)
	// Combine the 2 bytes into uint16_t (LSB first)
	uint16_t buttonStates = (uint16_t)spi_buf[0] | ((uint16_t)spi_buf[1] << 8);
	return buttonStates & 0x02;
}

uint16_t ShifterAnalog::G27ShifterButtonClient::getUserButtons() {
	// Read directly from spi_buf (DMA writes directly here, like SPI_Buttons)
	// Combine the 2 bytes into uint16_t (LSB first)
	uint16_t buttonStates = (uint16_t)spi_buf[0] | ((uint16_t)spi_buf[1] << 8);
	return buttonStates >> 4;
}

void ShifterAnalog::G27ShifterButtonClient::spiRxCompleted(SPIPort* port) {
	// DMA completed - data is now in spi_buf
	// Nothing else needed - data will be read on next readButtons() call
}
