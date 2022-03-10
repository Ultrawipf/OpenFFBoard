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
		 .id=CLSID_MAIN_FFBWHEEL,
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
		add_class<LocalButtons,ButtonSource>(0),
#endif
#ifdef SPIBUTTONS
		add_class<SPI_Buttons_1,ButtonSource>(1),
#endif
#ifdef SPIBUTTONS2
		add_class<SPI_Buttons_2,ButtonSource>(2),
#endif
#ifdef SHIFTERBUTTONS
		add_class<ShifterAnalog,ButtonSource>(3),
#endif
		add_class<PCF8574Buttons,ButtonSource>(4),
};

// Register possible analog sources (id 0-15)
const std::vector<class_entry<AnalogSource>> analog_sources =
{
#ifdef ANALOGAXES
		add_class<LocalAnalog,AnalogSource>(0),
#endif
};

FFBWheel::FFBWheel() :
		btn_chooser(button_sources),analog_chooser(analog_sources) // axes(1),
{
	// Creates the required no of axis (Default 1)
	effects_calc = std::make_unique<EffectsCalculator>();
	axes_manager = std::make_unique<AxesManager>(&control);
	axes_manager->setEffectsCalculator(effects_calc.get());
// Create the USB effects handler & pass in the effects calculator
	this->ffb = std::make_unique<HidFFB>();
	this->ffb->setEffectsCalculator(effects_calc.get());
	restoreFlash(); // Load parameters
	registerCommands();
}



FFBWheel::~FFBWheel() {
	clearBtnTypes();
}

/**
 * Read parameters from flash and restore settings
 */
void FFBWheel::restoreFlash(){

	Flash_Read(ADR_FFBWHEEL_BUTTONCONF, &this->btnsources);
	setBtnTypes(this->btnsources);

	Flash_Read(ADR_FFBWHEEL_ANALOGCONF, &this->ainsources);
	setAinTypes(this->ainsources);

	uint16_t conf1 = 0;
	if(Flash_Read(ADR_FFBWHEEL_CONF1,&conf1)){
		uint8_t rateidx = conf1 & 0x3;
		setReportRate(rateidx);
	}

}
/**
 * Save parameters to flash
 */
void FFBWheel::saveFlash(){

	Flash_Write(ADR_FFBWHEEL_BUTTONCONF,this->btnsources);
	Flash_Write(ADR_FFBWHEEL_ANALOGCONF,this->ainsources);

	uint8_t conf1 = 0;
	conf1 |= usb_report_rate_idx & 0x3;
	Flash_Write(ADR_FFBWHEEL_CONF1,conf1);
}


/**
 * Periodical update method. Called from main loop
 */

void FFBWheel::update(){
	if(control.request_update_disabled) {
		//logSerial("request update disabled");
		control.update_disabled = true;
		control.request_update_disabled = false;

	}
	if(control.update_disabled){
		//logSerial("Update disabled");
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



	// TODO Emulate a SOF timer...
	if(HAL_GetTick() - lastUsbReportTick > 0 && !control.usb_disabled){
		lastUsbReportTick = HAL_GetTick();
		control.usb_update_flag  = true;
	}

	// If either usb or timer triggered
	if(control.usb_update_flag || control.update_flag){
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
				this->btns.push_back(std::unique_ptr<ButtonSource>(btn));
		}
	}
}

void FFBWheel::addBtnType(uint16_t id){
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
void FFBWheel::clearAinTypes(){
	// Destruct all button sources

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
				this->analog_inputs.push_back(std::unique_ptr<AnalogSource>(ain));
		}
	}
}
void FFBWheel::addAinType(uint16_t id){
	for(auto &ain : this->analog_inputs){
		if(ain->getInfo().id == id){
			return;
		}
	}
	AnalogSource* ain = analog_chooser.Create(id);
	if(ain!=nullptr)
		this->analog_inputs.push_back(std::unique_ptr<AnalogSource>(ain));
}

uint32_t FFBWheel::getRate() {
	return this->ffb->getRate();
}

bool FFBWheel::getFfbActive(){
	return this->ffb->getFfbActive();
}

/**
 * Sends periodic gamepad reports of buttons and analog axes
 */
void FFBWheel::send_report(){

	// Read buttons
	reportHID.buttons = 0; // Reset buttons
	uint8_t shift = 0;
	if(btns.size() != 0){
		for(auto &btn : btns){
			uint64_t buf = 0;
			uint8_t amount = btn->readButtons(&buf);
			reportHID.buttons |= buf << shift;
			shift += amount;
		}
	}


	// Encoder
	//axes_manager->addAxesToReport(analogAxesReport, &count);

	std::vector<int32_t>* axes = axes_manager->getAxisValues();
	uint8_t count = 0;
	for(auto val : *axes){
		setHidReportAxis(&reportHID,count++,val);
	}

	// Fill remaining values with analog inputs
	for(auto &ain : analog_inputs){
		std::vector<int32_t>* axes = ain->getAxes();
		for(int32_t val : *axes){
			if(count >= analogAxisCount)
				break;
			setHidReportAxis(&reportHID,count++,val);
		}
	} // Fill rest
	for(;count<analogAxisCount; count++){
		setHidReportAxis(&reportHID,count,0);
	}

	/*
	 * Only send a new report if actually changed since last time or timeout and hid is ready
	 */
	if( (reportSendCounter++ > 100/usb_report_rate || (memcmp(&lastReportHID,&reportHID,sizeof(reportHID_t)) != 0) ) && tud_hid_n_ready(0)){
		tud_hid_report(0, reinterpret_cast<uint8_t*>(&reportHID), sizeof(reportHID_t));
		lastReportHID = reportHID;
		reportSendCounter = 0;
	}

}

/**
 * Changes the hid report rate based on the index for usb_report_rates
 */
void FFBWheel::setReportRate(uint8_t rateidx){
	rateidx = clip<uint8_t,uint8_t>(rateidx, 0,sizeof(usb_report_rates));
	usb_report_rate_idx = rateidx;
	usb_report_rate = usb_report_rates[rateidx]*HID_BINTERVAL;
}

/**
 * Generates the speed strings to display to the user
 */
std::string FFBWheel::usb_report_rates_names() {
		std::string s = "";
		for(uint8_t i = 0 ; i < sizeof(usb_report_rates);i++){
			s += std::to_string(1000/(HID_BINTERVAL*usb_report_rates[i])) + "Hz:"+std::to_string(i);
			if(i < sizeof(usb_report_rates)-1)
				s += ",";
		}
		return s;
	}

void FFBWheel::emergencyStop(){
	axes_manager->emergencyStop();
}

void FFBWheel::timerElapsed(TIM_HandleTypeDef* htim){
//	if(htim == this->timer_update){
//		control.update_flag = true;
//	}
}


/**
 * USB unplugged
 * Deactivates FFB
 */
void FFBWheel::usbSuspend(){
	if(control.usb_disabled)
		return;
	control.usb_disabled = true;
	ffb->stop_FFB();
	ffb->reset_ffb(); // Delete all effects
	axes_manager->usbSuspend();
}

/**
 * USB plugged in
 */
void FFBWheel::usbResume(){
#ifdef E_STOP_Pin
	if(control.emergency && HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) != GPIO_PIN_RESET){ // Reconnected after emergency stop
		control.emergency = false;
	}
#endif
	control.usb_disabled = false;
	axes_manager->usbResume();
}

void FFBWheel::usbInit(){
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_hid_conf,&usb_ffboard_strings_default);
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
		if(HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET){
			emergencyStop();
		}
	}
#endif
}

/*
 * Error handling
 */
void FFBWheel::errorCallback(Error &error, bool cleared){
	if(error.type == ErrorType::critical){
		if(!cleared){
			this->emergencyStop();
		}
	}
	if(!cleared){
		pulseErrLed();
	}
}
