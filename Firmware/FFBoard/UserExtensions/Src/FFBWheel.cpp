/*
 * FFBWheel.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#include "FFBWheel.h"
#include "FFBWheel_usb_init.h"
#include "voltagesense.h"

// Unique identifier for listing
ClassIdentifier FFBWheel::info = {
		 .name = "FFB Wheel" ,
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
		add_class<SPI_Buttons,ButtonSource>(),
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


// 0-63 valid ids
const std::vector<class_entry<MotorDriver>> motor_sources =
{

		add_class<MotorDriver,MotorDriver>(),
#ifdef TMC4671DRIVER
		add_class<TMC4671,MotorDriver>(),
#endif
#ifdef PWMDRIVER
		add_class<MotorPWM,MotorDriver>(),
#endif
};
// 0-63 valid ids
std::vector<class_entry<Encoder>> encoder_sources =
{
		add_class<Encoder,Encoder>(),
#ifdef LOCALENCODER
		add_class<EncoderLocal,Encoder>(),
#endif
};

// TODO class type for parser? (Simhub for example)
//////////////////////////////////////////////

FFBWheel::FFBWheel() :
		btn_chooser(button_sources),drv_chooser(motor_sources),enc_chooser(encoder_sources),analog_chooser(analog_sources)
{

	// Create HID FFB handler. Will receive all usb messages directly
	this->ffb = new HidFFB();

	// Setup a timer for updates faster than USB SOF
	extern TIM_HandleTypeDef htim4;
	this->timer_update = &htim4; // Timer setup with prescaler of sysclock
	this->timer_update->Instance->PSC = 95;
	this->timer_update->Instance->ARR = 500; // 250 = 4khz, 500 = 2khz...
	this->timer_update->Instance->CR1 = 1;
	HAL_TIM_Base_Start_IT(this->timer_update);

	restoreFlash(); // Load parameters
}

FFBWheel::~FFBWheel() {
	clearBtnTypes();
	delete drv;
	delete enc;
}

/*
 * Read parameters from flash and restore settings
 */
void FFBWheel::restoreFlash(){
	// read all constants
	uint16_t confint;
	if(Flash_Read(ADR_FFBWHEEL_CONFIG, &confint)){
		this->conf = FFBWheel::decodeConfFromInt(confint);
	}else{
		pulseErrLed();
	}

	setDrvType(this->conf.drvtype);
	setEncType(this->conf.enctype);

	Flash_Read(ADR_FFBWHEEL_BUTTONCONF, &this->btnsources);
	setBtnTypes(this->btnsources);

	Flash_Read(ADR_FFBWHEEL_ANALOGCONF, &this->ainsources);
	setAinTypes(this->ainsources);

	// Call restore methods for active button sources
	for(ButtonSource* btn : this->btns){
		btn->restoreFlash();
	}

	uint16_t cpr = 0;
	if(Flash_Read(ADR_TMC1_CPR, &cpr)){
		this->enc->setCpr(cpr);
	}else{
		pulseErrLed();
	}

	Flash_Read(ADR_FFBWHEEL_POWER, &this->power);
	Flash_Read(ADR_FFBWHEEL_DEGREES, &this->degreesOfRotation);
	nextDegreesOfRotation = degreesOfRotation;

	uint16_t esval;
	if(Flash_Read(ADR_FFBWHEEL_ENDSTOP,&esval)){
		this->fx_ratio_i = esval & 0xff;
		this->endstop_gain_i = (esval >> 8) & 0xff;
	}

	ffb->restoreFlash();
}
// Saves parameters to flash
void FFBWheel::saveFlash(){

	Flash_Write(ADR_FFBWHEEL_CONFIG,FFBWheel::encodeConfToInt(this->conf));
	Flash_Write(ADR_TMC1_CPR, enc->getCpr());
	Flash_Write(ADR_FFBWHEEL_POWER, this->power);
	Flash_Write(ADR_FFBWHEEL_DEGREES, this->degreesOfRotation);
	Flash_Write(ADR_FFBWHEEL_BUTTONCONF,this->btnsources);
	Flash_Write(ADR_FFBWHEEL_ANALOGCONF,this->ainsources);
	Flash_Write(ADR_FFBWHEEL_ENDSTOP,this->fx_ratio_i | (this->endstop_gain_i << 8));


	// TODO saving directly in persistenstorage
	// Call save methods for active button sources
	for(ButtonSource* btn : this->btns){
		btn->saveFlash();
	}

	for(AnalogSource* ain : this->analog_inputs){
		ain->saveFlash();
	}
	drv->saveFlash(); // Motor driver
	ffb->saveFlash(); // FFB handler
}

/*
 * Periodical update method. Called from main loop
 */
void FFBWheel::update(){

	if(drv == nullptr || enc == nullptr || emergency){
		pulseErrLed();
		return;
	}
	// If either usb SOF or timer triggered
	if(usb_update_flag || update_flag){
		int32_t lastTorque = torque;
		torque = 0;

		// Scale encoder value to set rotation range
		// Update a change of range only when new range is within valid range
		if(nextDegreesOfRotation != degreesOfRotation && abs(getEncValue(enc,nextDegreesOfRotation)) < 0x7fff){
			degreesOfRotation = nextDegreesOfRotation;
		}
		scaledEnc = getEncValue(enc,degreesOfRotation);

		update_flag = false;

		if(abs(scaledEnc) > 0xffff){
			// We are way off. Shut down
			drv->stop();
			pulseErrLed();
		}

		if(this->conf.drvtype == TMC4671::info.id){
			TMC4671* drv = static_cast<TMC4671*>(this->drv);
			drv->update();
			if(drv->estopTriggered){
				emergencyStop();
			}
//			if(!drv->initialized){
//				return;
//			}
		}

		speed = scaledEnc - lastScaledEnc;
		lastScaledEnc = scaledEnc;

		// Update USB Effects only on SOF
		if(usb_update_flag){

			usb_update_flag = false;
			effectTorque = ffb->calculateEffects(scaledEnc,1);

			if(abs(effectTorque) >= 0x7fff){
				pulseClipLed();
			}
			// Scale for power and endstop margin
			float effect_margin_scaler = ((float)fx_ratio_i/255.0);
			effectTorque *= ((float)this->power / (float)0x7fff) * effect_margin_scaler;
			//Send usb gamepad report
			if(++report_rate_cnt >= usb_report_rate){
				report_rate_cnt = 0;
				this->send_report();
			}
		}
		// Always check if endstop reached
		int32_t endstopTorque = updateEndstop();

		// Calculate total torque
		torque += effectTorque + endstopTorque;
		if(conf.invertX){ // Invert output torque if axis is flipped
			torque = -torque;
		}

		// Torque changed
		if(torque != lastTorque){
			// Update torque and clip
			torque = clip<int32_t,int32_t>(torque, -this->power, this->power);
			if(abs(torque) == power){
				pulseClipLed();
			}
			// Send to motor driver
			drv->turn(torque);
		}
	}

}

/*
 * Calculate soft endstop effect
 */
int16_t FFBWheel::updateEndstop(){
	int8_t clipdir = cliptest<int32_t,int32_t>(lastScaledEnc, -0x7fff, 0x7fff);
	if(clipdir == 0){
		return 0;
	}
	int32_t addtorque = 0;

	addtorque += clip<int32_t,int32_t>(abs(lastScaledEnc)-0x7fff,-0x7fff,0x7fff);
	addtorque *= (float)endstop_gain_i * 0.3f; // Apply endstop gain for stiffness
	addtorque *= -clipdir;

	return clip<int32_t,int32_t>(addtorque,-0x7fff,0x7fff);
}

void FFBWheel::setPower(uint16_t power){
	// Update hardware limits for TMC for safety
	if(this->conf.drvtype == TMC4671::info.id){
		TMC4671* drv = static_cast<TMC4671*>(this->drv);
		//tmclimits.pid_uq_ud = power;
		tmclimits.pid_torque_flux = power;
		drv->setLimits(tmclimits);
	}

	this->power = power;
}
uint16_t FFBWheel::getPower(){
	return this->power;
}

// create and setup a motor driver
void FFBWheel::setDrvType(uint8_t drvtype){

	if(!drv_chooser.isValidClassId(drvtype)){
		return;
	}

	Encoder* drvenc = dynamic_cast<Encoder*>(drv); // Cast old driver to encoder
	if(drv != nullptr){
		if(enc != nullptr && drvenc!=nullptr){
			for (auto it = encoder_sources.begin(); it != encoder_sources.end(); it++) {
				// Delete drv from encoder sources if present
				if(drvenc->getInfo().id == it->info.id){
					encoder_sources.erase(it);
					setEncType(0); // reset encoder
					break;
				}
			}
		}
		delete drv;
		drv = nullptr;
	}

	this->drv = drv_chooser.Create((uint16_t)drvtype);
	if(this->drv == nullptr){
		return;
	}
	this->conf.drvtype = drvtype;
	drvenc = dynamic_cast<Encoder*>(drv); // Cast new driver to encoder
	// Special handling for tmc4671.
	if(this->conf.drvtype == TMC4671::info.id){
		setupTMC4671();
	}

	// Add driver to encoder sources if also implements encoder

	if(drvenc != nullptr){
		if(!enc_chooser.isValidClassId(drv->getInfo().id)){
			//TMC4671* drv = static_cast<TMC4671*>(this->drv);
			//encoder_sources.push_back(add_class_ref<TMC4671,Encoder>(static_cast<Encoder*>(drv)));
			encoder_sources.push_back(make_class_entry<Encoder>(drv->getInfo(),drvenc));
			setEncType(drv->getInfo().id); // Auto preset driver as encoder
		}
	}
	drv->start();
}

// Special tmc setup methods
void FFBWheel::setupTMC4671(){

	TMC4671* drv = static_cast<TMC4671*>(this->drv);
	drv->setAddress(1);
	drv->initialize();
	drv->setPids(tmcpids);
	drv->setLimits(tmclimits);
	//drv->setBiquadFlux(fluxbq);
	drv->restoreFlash();

	if(tmcFeedForward){
		drv->setupFeedForwardTorque(torqueFFgain, torqueFFconst);
		drv->setupFeedForwardVelocity(velocityFFgain, velocityFFconst);
		drv->setFFMode(FFMode::torque);
	}
	// Enable driver

	drv->setMotionMode(MotionMode::torque);
}

void FFBWheel::setEncType(uint8_t enctype){


	if(enc_chooser.isValidClassId(enctype)){
		if(enc != nullptr && enc->getInfo().id != enctype && enctype != drv->getInfo().id && enc->getInfo().id != drv->getInfo().id){
			delete enc;
		}
		this->conf.enctype = (enctype);
		this->enc = enc_chooser.Create(enctype);
	}
	uint16_t cpr=0;
	if(Flash_Read(ADR_TMC1_CPR, &cpr)){
		this->enc->setCpr(cpr);
	}
	//this->enc->setPos(0); //Zero encoder
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


/*
 * Returns a scaled encoder value between -0x7fff and 0x7fff with a range of degrees
 */
int32_t FFBWheel::getEncValue(Encoder* enc,uint16_t degrees){
	if(enc == nullptr || degrees == 0){
		return 0x7fff; // Return center if no encoder present
	}
	float angle = 360.0*((float)enc->getPos()/(float)enc->getCpr());
	int32_t val = (0xffff / (float)degrees) * angle;

	// Flip encoder value (Also has to flip torque)
	if(conf.invertX){
		val = -val;
	}
	return val;
}

/*
 * Sends periodic gamepad reports of buttons and analog axes
 */
void FFBWheel::send_report(){
	extern USBD_HandleTypeDef hUsbDeviceFS;

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
	if(this->enc != nullptr){
		*(analogAxesReport[count++]) = clip(lastScaledEnc,-0x7fff,0x7fff);
	}

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

	USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, reinterpret_cast<uint8_t*>(&reportHID), sizeof(reportHID_t));

}

void FFBWheel::emergencyStop(){
	drv->stop();
	emergency = true;
}

void FFBWheel::timerElapsed(TIM_HandleTypeDef* htim){
	if(htim == this->timer_update){
		update_flag = true;
	}
}
// Called at 1khz. Triggers effect updates
void FFBWheel::SOF(){
	if(usb_disabled) // If previously disabled reenable
		usbResume();
	usb_update_flag = true;
	// USB clocked update callback
}

/*
 * USB unplugged
 */
void FFBWheel::usbSuspend(){
	if(usb_disabled)
		return;
	usb_disabled = true;
	ffb->stop_FFB();
	ffb->reset_ffb(); // Delete all effects
	if(drv != nullptr){
		drv->turn(0);
		drv->stop();
	}

}
void FFBWheel::usbResume(){
	usb_disabled = false;
	if(drv != nullptr){
		drv->start();
	}
}

void FFBWheel::usbInit(USBD_HandleTypeDef* hUsbDeviceFS){
	usbInit_HID_Wheel(hUsbDeviceFS);
}

// External interrupt pins
void FFBWheel::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == BUTTON_A_Pin){
		// Button down?
		if(HAL_GPIO_ReadPin(BUTTON_A_GPIO_Port, BUTTON_A_Pin) && this->enc != nullptr){
			this->enc->setPos(0);
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
 * Helper functions for encoding and decoding flash variables
 */
FFBWheelConfig FFBWheel::decodeConfFromInt(uint16_t val){
	// 0-7 Enc 8-15 Mot
	FFBWheelConfig conf;
	conf.enctype = ((val) & 0x3f);
	conf.drvtype = ((val >> 6) & 0x3f);
	conf.axes = (val >> 12) & 0x3;
	conf.invertX = (val >> 14) & 0x1;
	return conf;
}
uint16_t FFBWheel::encodeConfToInt(FFBWheelConfig conf){
	uint16_t val = (uint8_t)conf.enctype & 0x3f;
	val |= ((uint8_t)conf.drvtype & 0x3f) << 6;
	val |= (conf.axes & 0x03) << 12;
	val |= (conf.invertX & 0x1) << 14;
	return val;
}


