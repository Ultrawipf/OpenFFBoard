/*
 * FFBWheel.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick / Lidders / Vincent
 */

#include <FFBHIDMain.h>
#include "voltagesense.h"
#include "hid_device.h"
#include "tusb.h"
#include "usb_hid_ffb_desc.h"

#include "cmsis_os.h"
extern osThreadId_t defaultTaskHandle;

#ifdef TIM_FFB
extern TIM_HandleTypeDef TIM_FFB;
#endif

#ifndef OVERRIDE_FFBRATES
const FFBHIDMain::FFB_update_rates FFBHIDMain::ffbrates; // Default rates
#endif

//////////////////////////////////////////////


/**
 * setFFBEffectsCalc must be called in constructor of derived class to finish the setup
 */
FFBHIDMain::FFBHIDMain(uint8_t axisCount,bool hidAxis32b) :
		Thread("FFBMAIN", 312, 30),
		SelectableInputs(ButtonSource::all_buttonsources,AnalogSource::all_analogsources),
		axisCount(axisCount),hidAxis32b(hidAxis32b)
{
	if(hidAxis32b){
		reportHID = std::make_unique<HID_GamepadReport<int32_t>>();
	}else{
		reportHID = std::make_unique<HID_GamepadReport<int16_t>>();
	}
//	reportHID((hidAxis32b ? HID_GamepadReport<int32_t>() : HID_GamepadReport<int16_t>())),
//	lastReportHID((hidAxis32b ? HID_GamepadReport<int32_t>() : HID_GamepadReport<int16_t>())),
	restoreFlashDelayed(); // Load parameters
	registerCommands();

}

/**
 * MUST be called in the constructor of the derived class.
 * This finishes the construction because the constructor of this class will be called before the ffb and effects calc
 * objects are created in the derived class.
 */
void FFBHIDMain::setFFBEffectsCalc(std::shared_ptr<EffectsControlItf> ffb,std::shared_ptr<EffectsCalculator> effects_calc){
	this->ffb = ffb;
	this->effects_calc = effects_calc;
	axes_manager = std::make_unique<AxesManager>(&control,effects_calc);
	axes_manager->setAxisCount(axisCount);
	this->Start();
}


FFBHIDMain::~FFBHIDMain() {
	clearBtnTypes();
}

/**
 * Read parameters from flash and restore settings
 */
void FFBHIDMain::restoreFlash(){

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
void FFBHIDMain::saveFlash(){

	Flash_Write(ADR_FFBWHEEL_BUTTONCONF,this->btnsources);
	Flash_Write(ADR_FFBWHEEL_ANALOGCONF,this->ainsources);

	uint8_t conf1 = 0;
	conf1 |= usb_report_rate_idx & 0x7;
	Flash_Write(ADR_FFBWHEEL_CONF1,conf1);
}



void FFBHIDMain::Run(){
#ifdef E_STOP_Pin
	bool estopState = HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET;
	if(estopState){ // Estop pressed at startup
		emergencyStop(!estopState);
//		control.emergency = true; // Immediately enter emergency state but without notifying other classes yet
		lastEstop = HAL_GetTick();
	}
#endif
#ifdef TIM_FFB
	HAL_TIM_Base_Start_IT(&TIM_FFB); // Start generating updates
#endif
	while(true){
#ifndef TIM_FFB
		while(ffb_rate_counter++ <= ffb_rate_divider){
			Delay(1);
		}
		ffb_rate_counter = 0;
#else
		WaitForNotification();
#endif
		updateControl();
	}
}

/**
 * Main update loop
 */
void FFBHIDMain::updateControl(){
	if(control.request_update_disabled) {
		//logSerial("request update disabled");
		control.update_disabled = true;
		control.request_update_disabled = false;

	}
	if(control.update_disabled){
		//logSerial("Update disabled");
		return;
	}

	if(control.resetEncoder){
		control.resetEncoder = false;
		axes_manager->resetPosZero();
	}

	//debugpin.set();
	axes_manager->update();

	if(++report_rate_cnt >= usb_report_rate){
		report_rate_cnt = 0;
		this->send_report();
	}
	if(!control.emergency){
		axes_manager->updateTorque();

	}else{
		pulseClipLed();
	}
	//debugpin.reset();
}


uint32_t FFBHIDMain::getRate() {
	return this->ffb->getRate();
}

bool FFBHIDMain::getFfbActive(){
	return this->ffb->getFfbActive();
}

/**
 * Sends periodic gamepad reports of buttons and analog axes
 */
void FFBHIDMain::send_report(){
	// Check if HID command interface wants to send something and allow that if we did not skip too many reports
	if(!tud_hid_n_ready(0) ||  ((reportSendCounter++ < usb_report_rate*2) && this->hidCommands->waitingToSend())){
		return;
	}
	//Try semaphore
//	if(!sourcesSem.Take(10)){
//		return;
//	}

	// Read buttons
	uint64_t b = 0;
	SelectableInputs::getButtonValues(b);
	reportHID->setButtons(b);

	// Encoder
	//axes_manager->addAxesToReport(analogAxesReport, &count);

	std::vector<int32_t>* axes = axes_manager->getAxisValues();
	uint8_t count = 0;
	for(auto val : *axes){
		if(!hidAxis32b){
			val = val >> 16; // Scale to 16b
		}else{
			val = val >> (32-HIDAXISRES_32B_BITS); // Scale if less than 32b
		}
		//setHidReportAxis(&reportHID,count++,val);
		reportHID->setHidReportAxis(count++, val);
	}

	// Fill remaining values with analog inputs
	axes = SelectableInputs::getAnalogValues();
	for(int32_t val : *axes){
		if(count >= analogAxisCount)
			break;
		if((count < MAX_AXIS) && hidAxis32b)
			val = val << (HIDAXISRES_32B_BITS-16); // Shift up 16 bit to fill 32b value. Primary axis is 32b
		reportHID->setHidReportAxis(count++, val);
	}

//	sourcesSem.Give();
	// Fill rest
	for(;count<analogAxisCount; count++){
		//setHidReportAxis(&reportHID,count,0);
		reportHID->setHidReportAxis(count, 0);
	}


	/*
	 * Only send a new report if actually changed since last time or timeout and hid is ready
	 */
	if( (reportSendCounter > 100/usb_report_rate || reportHID->changed()) )
	{
		tud_hid_report(0, reportHID->getBuffer(), reportHID->getLength());
		reportHID->swap(); // Report has changed and was sent. Swap buffers.
		reportSendCounter = 0;
	}

}

/**
 * Returns current FFB update loop frequency in Hz
 */
float FFBHIDMain::getCurFFBFreq(){
	return ffbrates.basefreq/((uint32_t)ffbrates.dividers[usb_report_rate_idx].basediv);
}

/**
 * Changes the hid report rate based on the index for usb_report_rates
 */
void FFBHIDMain::setReportRate(uint8_t rateidx){
	uint32_t usbrate_base = TUD_OPT_HIGH_SPEED ? 8000 : 1000;
	if(tud_connected()){ // Get either actual rate or max supported rate if not connected
		usbrate_base = tud_speed_get() == TUSB_SPEED_HIGH ? 8000 : 1000; // Only FS and HS supported
	}

	rateidx = clip<uint8_t,uint8_t>(rateidx, 0,ffbrates.dividers.size());
	usb_report_rate_idx = rateidx;


	// Either limit using rate counter or HW timer if present.
#ifdef TIM_FFB
	TIM_FFB.Instance->ARR = ((1000000*(uint32_t)ffbrates.dividers[rateidx].basediv)/ffbrates.basefreq); // Assumes 1µs timer steps
#else
	ffb_rate_divider = ffbrates.dividers[rateidx].basediv;
#endif
	usb_report_rate = ffbrates.dividers[rateidx].hiddiv*HID_BINTERVAL;
	// Divide report rate down if above actual usb rate
	while(((ffbrates.basefreq / (uint32_t)ffbrates.dividers[rateidx].basediv) / usb_report_rate)  > usbrate_base){
		usb_report_rate++;
	}

	// Pass updated rate to other classes to update filters
	float newRate = getCurFFBFreq();
	if(ffb)
		ffb->updateSamplerate(newRate);
	if(axes_manager)
		axes_manager->updateSamplerate(newRate);
}

/**
 * Generates the speed strings to display to the user
 */
std::string FFBHIDMain::usb_report_rates_names() {
	std::string s = "";
	uint32_t usbrate_base = TUD_OPT_HIGH_SPEED ? 8000 : 1000;
	if(tud_connected()){ // Get either actual rate or max supported rate if not connected
		usbrate_base = tud_speed_get() == TUSB_SPEED_HIGH ? 8000 : 1000; // Only FS and HS supported
	}
	for(uint8_t i = 0 ; i < ffbrates.dividers.size();i++){
		uint32_t updatefreq = ffbrates.basefreq/((uint32_t)ffbrates.dividers[i].basediv);
		uint32_t hidrate = (HID_BINTERVAL*ffbrates.dividers[i].hiddiv);
		while((updatefreq/hidrate)  > usbrate_base){ // Fall back if usb rate is still higher than supported
			hidrate++;
		}
		uint32_t hidfreq = updatefreq/hidrate;
		s += "FFB "+std::to_string(updatefreq) + "Hz\nUSB " + std::to_string(hidfreq) + "Hz:"+std::to_string(i);
		if(i < ffbrates.dividers.size()-1)
			s += ",";
	}
	return s;
}

void FFBHIDMain::emergencyStop(bool reset){
	control.emergency = !reset;
	axes_manager->emergencyStop(reset);
}


/**
 * USB unplugged
 * Deactivates FFB
 */
void FFBHIDMain::usbSuspend(){
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
void FFBHIDMain::usbResume(){
#ifdef E_STOP_Pin
	if(control.emergency && HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) != GPIO_PIN_RESET){ // Reconnected after emergency stop
		control.emergency = false;
	}
#endif
	setReportRate(this->usb_report_rate_idx);
	control.usb_disabled = false;
	axes_manager->usbResume();
}


// External interrupt pins
void FFBHIDMain::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == BUTTON_A_Pin){
		// Button down?
		if(HAL_GPIO_ReadPin(BUTTON_A_GPIO_Port, BUTTON_A_Pin)){
			this->control.resetEncoder = true;
		}
	}
#ifdef E_STOP_Pin
	if(GPIO_Pin == E_STOP_Pin){ // Emergency stop. low active
//		if(HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET){
		bool estopPinState = HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET;
		if(HAL_GetTick()-lastEstop > 1000 && estopPinState != this->control.emergency){ // Long debounce
			lastEstop = HAL_GetTick();
			if(estopPinState){
				emergencyStop(false);
			}else if(allowEstopReset){
				emergencyStop(true);
			}

		}
//		}
	}
#endif
}

/*
 * Error handling
 */
void FFBHIDMain::errorCallback(const Error &error, bool cleared){
	if(error.type == ErrorType::critical){
		if(!cleared){
			this->emergencyStop(true);
		}
	}
	if(error.code == ErrorCode::emergencyStop){
		this->emergencyStop(cleared); // Clear Estop
	}
	if(!cleared){
		pulseErrLed();
	}
}

#ifdef TIM_FFB
void FFBHIDMain::timerElapsed(TIM_HandleTypeDef* htim){
	if(htim == &TIM_FFB){
		NotifyFromISR();
	}
}
#endif

