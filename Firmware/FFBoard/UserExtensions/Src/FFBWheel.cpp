/*
 * FFBWheel.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick / Lidders
 */

#include "FFBWheel.h"
#include "voltagesense.h"
#include "hid_device.h"
#include "tusb.h"
#include "usb_hid_ffb_desc.h"

// Unique identifier for listing
ClassIdentifier FFBWheel::info = {
		 .name = "FFB Wheel" , // Leave as wheel for now
		 .id=1
 };

const ClassIdentifier FFBWheel::getInfo(){
	return info;
}

//////////////////////////////////////////////
/*
 * Sources for class choosers here
 */
// Register possible button sources (id 0-15)
const std::vector<class_entry<ButtonSource>> button_sources =
{
#ifdef LOCALBUTTONS
		add_class<LocalButtons,ButtonSource>(),
#endif
#ifdef SPIBUTTONS
		add_class<SPI_Buttons_1,ButtonSource>(),
#endif
#ifdef SPIBUTTONS2
		add_class<SPI_Buttons_2,ButtonSource>(),
#endif
#ifdef SHIFTERBUTTONS
		add_class<ShifterAnalog,ButtonSource>(),
#endif
};

// Register possible analog sources (id 0-15)
const std::vector<class_entry<AnalogSource>> analog_sources =
{
#ifdef ANALOGAXES
		add_class<LocalAnalog,AnalogSource>(),
#endif
};

FFBWheel::FFBWheel() :
		btn_chooser(button_sources),analog_chooser(analog_sources) // axes(1),
{
	// Creates the required no of axis (Default 1)
	effects_calc = new EffectsCalculator();
	axes_manager = new AxesManager(&control);
	axes_manager->setEffectsCalculator(effects_calc);
// Create the USB effects handler & pass in the effects calculator
	this->ffb = new HidFFB();
	this->ffb->setEffectsCalculator(effects_calc);
	this->ffb->setHIDCommandHandler(axes_manager);
	restoreFlash(); // Load parameters
}



FFBWheel::~FFBWheel() {
	clearBtnTypes();
	delete axes_manager;
	delete ffb;
	delete effects_calc;
}

/*
 * Read parameters from flash and restore settings
 */
void FFBWheel::restoreFlash(){

	axes_manager->restoreFlash();
	Flash_Read(ADR_FFBWHEEL_BUTTONCONF, &this->btnsources);
	setBtnTypes(this->btnsources);

	Flash_Read(ADR_FFBWHEEL_ANALOGCONF, &this->ainsources);
	setAinTypes(this->ainsources);

	// Call restore methods for active button sources
	for(ButtonSource* btn : this->btns){
		btn->restoreFlash();
	}
}
// Saves parameters to flash
void FFBWheel::saveFlash(){

	Flash_Write(ADR_FFBWHEEL_BUTTONCONF,this->btnsources);
	Flash_Write(ADR_FFBWHEEL_ANALOGCONF,this->ainsources);

	// TODO saving directly in persistenstorage
	// Call save methods for active button sources
	for(ButtonSource* btn : this->btns){
		btn->saveFlash();
	}

	for(AnalogSource* ain : this->analog_inputs){
		ain->saveFlash();
	}

	axes_manager->saveFlash();
}


/*
 * Periodical update method. Called from main loop
 */
void FFBWheel::update(){
	if(control.request_update_disabled) {
		logSerial("request update disabled");
		control.update_disabled = true;
		control.request_update_disabled = false;
	}
	if(control.update_disabled){
		logSerial("Update disabled");
		return;
	}
	if(control.emergency){
		pulseErrLed();
		return;
	}
	if(control.resetEncoder){
		control.resetEncoder = false;
		axes_manager->resetPosZero();
	}


	// Emulate a SOF timer... TODO
	if(HAL_GetTick() - lastUsbReportTick > 0 && !control.usb_disabled){
		lastUsbReportTick = HAL_GetTick();
		control.usb_update_flag  = true;
	}

	// If either usb or timer triggered
	if(control.usb_update_flag || control.update_flag){
//		logSerial("Updating");
		axes_manager->update();
		control.update_flag = false;
		if(control.usb_update_flag){
			control.usb_update_flag = false;
			if(++report_rate_cnt >= usb_report_rate){
					report_rate_cnt = 0;
					this->send_report();
			}
		}
		axes_manager->updateTorque();
	}
}


// Buttons
void FFBWheel::clearBtnTypes(){
	// Destruct all button sources
	for(ButtonSource* btn : this->btns){
		delete btn;
	}
	this->btns.clear();
}

void FFBWheel::setBtnTypes(uint16_t btntypes){
	this->btnsources = btntypes;
	clearBtnTypes();
	for(uint8_t id = 0;id<16;id++){
		if((btntypes >> id) & 0x1){
			// Matching flag
			ButtonSource* btn = btn_chooser.Create(id);
			if(btn!=nullptr)
				this->btns.push_back(btn);
		}
	}
}

void FFBWheel::addBtnType(uint16_t id){
	for(ButtonSource* btn : this->btns){
		if(btn->getInfo().id == id){
			return;
		}
	}
	ButtonSource* btn = btn_chooser.Create(id);
	if(btn!=nullptr)
		this->btns.push_back(btn);
}

// Analog
void FFBWheel::clearAinTypes(){
	// Destruct all button sources
	for(AnalogSource* ain : this->analog_inputs){
		delete ain;
	}
	this->analog_inputs.clear();
}

void FFBWheel::setAinTypes(uint16_t aintypes){
	this->ainsources = aintypes;
	clearAinTypes();
	for(uint8_t id = 0;id<16;id++){
		if((aintypes >> id) & 0x1){
			// Matching flag
			AnalogSource* ain = analog_chooser.Create(id);
			if(ain!=nullptr)
				this->analog_inputs.push_back(ain);
		}
	}
}
void FFBWheel::addAinType(uint16_t id){
	for(AnalogSource* ain : this->analog_inputs){
		if(ain->getInfo().id == id){
			return;
		}
	}
	AnalogSource* ain = analog_chooser.Create(id);
	if(ain!=nullptr)
		this->analog_inputs.push_back(ain);
}

uint32_t FFBWheel::getRate() {
	return this->ffb->getRate();
}

bool FFBWheel::getFfbActive(){
	return this->ffb->getFfbActive();
}

/*
 * Sends periodic gamepad reports of buttons and analog axes
 */
void FFBWheel::send_report(){

	// Read buttons
	reportHID.buttons = 0; // Reset buttons
	uint8_t shift = 0;
	if(btns.size() != 0){
		for(ButtonSource* btn : btns){
			uint32_t buf = 0;
			btn->readButtons(&buf);
			reportHID.buttons |= buf << shift;
			shift += btn->getBtnNum();
		}
	}

	uint8_t count = 0;
	// Encoder
	axes_manager->addAxesToReport(analogAxesReport, &count);

	for(AnalogSource* ain : analog_inputs){
		std::vector<int32_t>* axes = ain->getAxes();
		for(int32_t val : *axes){
			if(count >= analogAxisCount)
				break;
			*analogAxesReport[count++] = val;
		}
	} // Fill rest
	for(;count<analogAxisCount; count++){
		*analogAxesReport[count] = 0;
	}

	/*
	 * Only send a new report if actually changed since last time or timeout
	 */
	if(reportSendCounter++ > 100 || (memcmp(&lastReportHID,&reportHID,sizeof(reportHID_t)) != 0) ){
		tud_hid_report(0, reinterpret_cast<uint8_t*>(&reportHID), sizeof(reportHID_t));
		lastReportHID = reportHID;
		reportSendCounter = 0;
	}

}

void FFBWheel::emergencyStop(){
	axes_manager->emergencyStop();
}

void FFBWheel::timerElapsed(TIM_HandleTypeDef* htim){
	if(htim == this->timer_update){
		control.update_flag = true;
	}
}


/*
 * USB unplugged
 */
void FFBWheel::usbSuspend(){
	if(control.usb_disabled)
		return;
	control.usb_disabled = true;
	ffb->stop_FFB();
	ffb->reset_ffb(); // Delete all effects
	axes_manager->usbSuspend();
}

void FFBWheel::usbResume(){
	control.usb_disabled = false;
	axes_manager->usbResume();
}

void FFBWheel::usbInit(){
	//usbInit_HID_Wheel(hUsbDeviceFS);
	this->usbdev = new USBdevice(&usb_devdesc_ffboard_composite,usb_cdc_hid_conf,&usb_ffboard_strings_default);
	UsbHidHandler::setHidDesc(hid_ffb_desc);
	usbdev->registerUsb();
}

// External interrupt pins
void FFBWheel::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == BUTTON_A_Pin){
		// Button down?
		if(HAL_GPIO_ReadPin(BUTTON_A_GPIO_Port, BUTTON_A_Pin)){
			this->control.resetEncoder = true;
		}
	}
#ifdef E_STOP_Pin
	if(GPIO_Pin == E_STOP_Pin){ // Emergency stop. low active
		if(HAL_GPIO_ReadPin(BUTTON_A_GPIO_Port, BUTTON_A_Pin) == GPIO_PIN_RESET){
			emergencyStop();
		}
	}
#endif
}

/*
 * Error handling
 */
void FFBWheel::errorCallback(Error_t &error, bool cleared){
	if(error.type == ErrorType::critical){
		if(!cleared){
			this->emergencyStop();
		}
	}
	if(!cleared){
		pulseErrLed();
	}
}
