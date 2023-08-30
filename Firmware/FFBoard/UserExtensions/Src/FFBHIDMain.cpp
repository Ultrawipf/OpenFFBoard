/*
 * FFBWheel.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick / Lidders
 */

#include <FFBHIDMain.h>
#include "voltagesense.h"
#include "hid_device.h"
#include "tusb.h"
#include "usb_hid_ffb_desc.h"

#include "SPIButtons.h"
#include "CanButtons.h"
#include "LocalButtons.h"
#include <ShifterAnalog.h>
#include "PCF8574.h"

#include "LocalAnalog.h"
#include "CanAnalog.h"
#include "ADS111X.h"

#include "cmsis_os.h"
extern osThreadId_t defaultTaskHandle;

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
#ifdef PCF8574BUTTONS
		add_class<PCF8574Buttons,ButtonSource>(4),
#endif
#ifdef CANBUTTONS
		add_class<CanButtons,ButtonSource>(5),
#endif
};

// Register possible analog sources (id 0-15)
const std::vector<class_entry<AnalogSource>> analog_sources =
{
#ifdef ANALOGAXES
		add_class<LocalAnalog,AnalogSource>(0),
#endif
#ifdef CANANALOG
		add_class<CanAnalog<8>,AnalogSource>(1),
#endif
#ifdef ADS111XANALOG
		add_class<ADS111X_AnalogSource,AnalogSource>(2),
#endif
};

/**
 * setFFBEffectsCalc must be called in constructor of derived class to finish the setup
 */
FFBHIDMain::FFBHIDMain(uint8_t axisCount) :
		Thread("FFBMAIN", 256, 30),axisCount(axisCount),btn_chooser(button_sources),analog_chooser(analog_sources)
{

	restoreFlash(); // Load parameters
	registerCommands();


#ifdef E_STOP_Pin
	bool estopState = HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET;
	if(estopState){ // Estop pressed at startup
		emergencyStop(!estopState);
		lastEstop = HAL_GetTick();
	}
#endif

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
	conf1 |= usb_report_rate_idx & 0x3;
	Flash_Write(ADR_FFBWHEEL_CONF1,conf1);
}



void FFBHIDMain::Run(){
	while(true){
		Delay(1);
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


// Buttons
void FFBHIDMain::clearBtnTypes(){
	// Destruct all button sources

	this->btns.clear();
}

void FFBHIDMain::setBtnTypes(uint16_t btntypes){
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

void FFBHIDMain::addBtnType(uint16_t id){
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
void FFBHIDMain::clearAinTypes(){
	// Destruct all button sources

	this->analog_inputs.clear();
}

void FFBHIDMain::setAinTypes(uint16_t aintypes){
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
void FFBHIDMain::addAinType(uint16_t id){
	for(auto &ain : this->analog_inputs){
		if(ain->getInfo().id == id){
			return;
		}
	}
	AnalogSource* ain = analog_chooser.Create(id);
	if(ain!=nullptr)
		this->analog_inputs.push_back(std::unique_ptr<AnalogSource>(ain));
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
	if(!tud_hid_n_ready(0) ||  (reportSendCounter++ < usb_report_rate*2 && this->hidCommands->waitingToSend())){
		return;
	}
	//Try semaphore
	if(!sourcesSem.Take(10)){
		return;
	}
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
	}
	sourcesSem.Give();
	// Fill rest
	for(;count<analogAxisCount; count++){
		setHidReportAxis(&reportHID,count,0);
	}


	/*
	 * Only send a new report if actually changed since last time or timeout and hid is ready
	 */
	if( (reportSendCounter > 100/usb_report_rate || (memcmp(&lastReportHID,&reportHID,sizeof(reportHID_t)) != 0) ))
	{


	tud_hid_report(0, reinterpret_cast<uint8_t*>(&reportHID), sizeof(reportHID_t));
	lastReportHID = reportHID;
	reportSendCounter = 0;

	}

}

/**
 * Changes the hid report rate based on the index for usb_report_rates
 */
void FFBHIDMain::setReportRate(uint8_t rateidx){
	rateidx = clip<uint8_t,uint8_t>(rateidx, 0,sizeof(usb_report_rates));
	usb_report_rate_idx = rateidx;
	usb_report_rate = usb_report_rates[rateidx]*HID_BINTERVAL;
}

/**
 * Generates the speed strings to display to the user
 */
std::string FFBHIDMain::usb_report_rates_names() {
		std::string s = "";
		for(uint8_t i = 0 ; i < sizeof(usb_report_rates);i++){
			s += std::to_string(1000/(HID_BINTERVAL*usb_report_rates[i])) + "Hz:"+std::to_string(i);
			if(i < sizeof(usb_report_rates)-1)
				s += ",";
		}
		return s;
	}

void FFBHIDMain::emergencyStop(bool reset){
	control.emergency = !reset;
	axes_manager->emergencyStop(reset);
}

//void FFBHIDMain::timerElapsed(TIM_HandleTypeDef* htim){
//
//}


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

