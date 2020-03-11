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
		add_class<LocalButtons,ButtonSource>(),
		add_class<SPI_Buttons,ButtonSource>(),
		add_class<ShifterG29,ButtonSource>()
};

// 0-63 valid ids
const std::vector<class_entry<MotorDriver>> motor_sources =
{
		add_class<TMC4671,MotorDriver>(),
};
// 0-63 valid ids
std::vector<class_entry<Encoder>> encoder_sources =
{
		add_class<Encoder,Encoder>(),
		add_class<EncoderLocal,Encoder>()
};
//////////////////////////////////////////////

FFBWheel::FFBWheel() : btn_chooser(button_sources),drv_chooser(motor_sources),enc_chooser(encoder_sources) {

	// Create HID FFB handler. Will receive all usb messages directly
	this->ffb = new HidFFB();

	// Setup a timer
	extern TIM_HandleTypeDef htim4;
	this->timer_update = &htim4; // Timer setup with prescaler of sysclock
	this->timer_update->Instance->ARR = 250;
	this->timer_update->Instance->CR1 = 1;
	HAL_TIM_Base_Start_IT(this->timer_update);

	restoreFlash(); // Load parameters
}

FFBWheel::~FFBWheel() {
	clearBtnTypes();
	delete drv;
	delete enc;
}


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

	// Call restore methods for active button sources
	for(ButtonSource* btn : this->btns){
		btn->restoreFlash();
	}

	uint16_t ppr = 0;
	if(Flash_Read(ADR_TMC1_PPR, &ppr)){
		this->enc->setPpr(ppr);
	}else{
		pulseErrLed();
	}

	Flash_Read(ADR_FFBWHEEL_POWER, &this->power);
	Flash_Read(ADR_FFBWHEEL_DEGREES, &this->degreesOfRotation);

	uint16_t aconfint;
	if(Flash_Read(ADR_FFBWHEEL_ANALOGCONF,&aconfint)){
		this->aconf = FFBWheel::decodeAnalogConfFromInt(aconfint);
	}
}
// Saves parameters to flash
void FFBWheel::saveFlash(){
	if(this->conf.drvtype == TMC4671::info.id){
		TMC4671* drv = static_cast<TMC4671*>(this->drv);
		//save motor config
		uint16_t motint = TMC4671::encodeMotToInt(drv->conf.motconf);
		Flash_Write(ADR_TMC1_MOTCONF, motint);


	}
	Flash_Write(ADR_FFBWHEEL_CONFIG,FFBWheel::encodeConfToInt(this->conf));
	Flash_Write(ADR_TMC1_PPR, enc->getPpr());
	Flash_Write(ADR_FFBWHEEL_POWER, this->power);
	Flash_Write(ADR_FFBWHEEL_DEGREES, this->degreesOfRotation);
	Flash_Write(ADR_FFBWHEEL_BUTTONCONF,this->btnsources);
	Flash_Write(ADR_FFBWHEEL_ANALOGCONF, FFBWheel::encodeAnalogConfToInt(this->aconf));

	// Call save methods for active button sources
	for(ButtonSource* btn : this->btns){
		btn->saveFlash();
	}

	if(drv->getInfo().id == TMC4671::info.id){
		TMC4671* drv = static_cast<TMC4671*>(this->drv);
		drv->saveFlash();
	}
}

/*
 * Periodical update method. Called from main loop
 */
void FFBWheel::update(){
	int16_t lasttorque = endstopTorque;
	bool updateTorque = false;

	if(drv == nullptr || enc == nullptr){
		pulseErrLed();
		return;
	}

	if(usb_update_flag || update_flag){

		torque = 0;
		scaledEnc = getEncValue(enc,degreesOfRotation);

		update_flag = false;

		if(abs(scaledEnc) > 0xffff){
			// We are way off. Shut down
			drv->stop();
			pulseErrLed();
		}
		endstopTorque = updateEndstop();

		if(this->conf.drvtype == TMC4671::info.id){
			TMC4671* drv = static_cast<TMC4671*>(this->drv);
			drv->update();
		}
	}
	if(usb_update_flag){
		speed = scaledEnc - lastScaledEnc;
		lastScaledEnc = scaledEnc;

		usb_update_flag = false;
		torque = ffb->calculateEffects(scaledEnc,1);
		if(abs(torque) >= 0x7fff){
			pulseClipLed();
		}
		if(endstopTorque == 0 || (endstopTorque > 0 && torque > 0) || (endstopTorque < 0 && torque < 0))
		{
			torque *= 0.8*((float)this->power / (float)0x7fff); // Scale for power
			updateTorque = true;
		}
		this->send_report();
	}



	if(endstopTorque!=lasttorque || updateTorque){
		// Update torque
		torque = torque+endstopTorque;
		//Invert direction for now
		torque = clip<int32_t,int16_t>(torque, -this->power, this->power);
		if(abs(torque) == power){
			pulseClipLed();
		}
		drv->turn(torque);
	}
}


int16_t FFBWheel::updateEndstop(){
	int8_t clipdir = cliptest<int32_t,int32_t>(lastScaledEnc, -0x7fff, 0x7fff);
	if(clipdir == 0){
		return 0;
	}
	int32_t addtorque = 0;

	addtorque += clip<int32_t,int32_t>(abs(lastScaledEnc)-0x7fff,-0x7fff,0x7fff);
	addtorque *= endstop_gain;
	addtorque *= -clipdir;


	return clip<int32_t,int32_t>(addtorque,-0x7fff,0x7fff);
}


// create and setup a motor driver
void FFBWheel::setDrvType(uint8_t drvtype){

	if(!drv_chooser.isValidClassId(drvtype)){
		return;
	}

	if(drv != nullptr){
		delete drv;
		drv = nullptr;
	}

	//
	this->drv = drv_chooser.Create((uint16_t)drvtype);
	if(this->drv == nullptr){
		return;
	}
	this->conf.drvtype = drvtype;

	// Special handling for tmc4671.
	if(this->conf.drvtype == TMC4671::info.id){
		setupTMC4671();

		// Add tmc to encoder sources if not present
		if(!enc_chooser.isValidClassId(TMC4671::info.id)){
			TMC4671* drv = static_cast<TMC4671*>(this->drv);
			encoder_sources.push_back(add_class_ref<TMC4671,Encoder>(static_cast<Encoder*>(drv)));
		}
	}else{
		// Check if encoder sources must be removed
		for (auto it = encoder_sources.begin(); it != encoder_sources.end(); it++) {
			// Delete tmc from encoder sources if present
			if(it->info.id == TMC4671::info.id){
				encoder_sources.erase(it);
				break;
			}
		}
	}
}

// Special tmc setup methods
void FFBWheel::setupTMC4671(){

	TMC4671* drv = static_cast<TMC4671*>(this->drv);
	drv->setAddress(1);
	drv->setPids(tmcpids);
	drv->restoreFlash();

	if(tmcFeedForward){
		drv->setupFeedForwardTorque(torqueFFgain, torqueFFconst);
		drv->setupFeedForwardVelocity(velocityFFgain, velocityFFconst);
		drv->setFFMode(FFMode::torque);
	}
	drv->setPhiEtype(PhiE::abn);
	drv->initialize();
	drv->setMotionMode(MotionMode::torque);
}

void FFBWheel::setEncType(uint8_t enctype){


	if(enc_chooser.isValidClassId(enctype)){
		if(enc != nullptr){
			delete enc;
			enc = nullptr;
		}
		this->conf.enctype = (enctype);
		this->enc = enc_chooser.Create(enctype);
	}
}

ButtonSource* FFBWheel::getBtnSrc(uint16_t id){
	for(ButtonSource* btn : this->btns){
		if(btn->getInfo().id == id){
			return btn;
		}
	}
	return nullptr;
}

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


void FFBWheel::adcUpd(volatile uint32_t* ADC_BUF){
	for(uint8_t i = 0;i<ADC_PINS;i++){
		this->adc_buf[i] = ADC_BUF[i+ADC_CHAN_FPIN];

		if(this->aconf.offsetmode == AnalogOffset::LOWER){
			this->adc_buf[i] = clip(this->adc_buf[i] << 5, 0, 0xfff);
		}else if(this->aconf.offsetmode == AnalogOffset::UPPER){
			this->adc_buf[i] = clip(this->adc_buf[i] - 0x7ff, 0, 0xfff) << 5;
		}

	}
}

int32_t FFBWheel::getEncValue(Encoder* enc,uint16_t degrees){
	if(enc == nullptr){
		return 0x7fff; // Return center if no encoder present
	}
	float angle = 360.0*((float)enc->getPos()/(float)enc->getPosCpr());
	int32_t val = (0xffff / (float)degrees) * angle;
	return val;
}


void FFBWheel::send_report(){
	extern USBD_HandleTypeDef hUsbDeviceFS;

	// Read buttons
	reportHID.buttons = 0; // Reset buttons
	uint8_t shift = 0;
	if(btns.size() != 0)
		for(ButtonSource* btn : btns){
			uint32_t buf = 0;
			btn->readButtons(&buf);
			reportHID.buttons |= buf << shift;
			shift += btn->getBtnNum();
		}

	// Encoder
	reportHID.X = clip(lastScaledEnc,-0x7fff,0x7fff);
	// Analog values read by DMA
	uint16_t analogMask = this->aconf.analogmask;
	reportHID.Y 	=  (analogMask & 0x01) ? ((adc_buf[0] & 0xFFF) << 4)	-0x7fff : 0;
	reportHID.Z		=  (analogMask & 0x01<<1) ? ((adc_buf[1] & 0xFFF) << 4)	-0x7fff : 0;
	reportHID.RX	=  (analogMask & 0x01<<2) ? ((adc_buf[2] & 0xFFF) << 4)	-0x7fff : 0;
	reportHID.RY	=  (analogMask & 0x01<<3) ? ((adc_buf[3] & 0xFFF) << 4)	-0x7fff : 0;
	reportHID.RZ	=  (analogMask & 0x01<<4) ? ((adc_buf[4] & 0xFFF) << 4)	-0x7fff : 0;
	reportHID.Slider=  (analogMask & 0x01<<5) ? ((adc_buf[5] & 0xFFF) << 4)	-0x7fff : 0;

	USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, reinterpret_cast<uint8_t*>(&reportHID), sizeof(reportHID_t));

}

void FFBWheel::timerElapsed(TIM_HandleTypeDef* htim){
	if(htim == this->timer_update){
		update_flag = true;
	}
}

void FFBWheel::usbInit(){
	usbInit_HID_Wheel();
}
void FFBWheel::SOF(){
	usb_update_flag = true;
	// USB clocked update callback
}

FFBWheelConfig FFBWheel::decodeConfFromInt(uint16_t val){
	// 0-7 Enc 8-15 Mot
	FFBWheelConfig conf;
	conf.enctype = ((val) & 0x3f);
	conf.drvtype = ((val >> 6) & 0x3f);
	conf.axes = (val >> 12) & 0x3;
	return conf;
}
uint16_t FFBWheel::encodeConfToInt(FFBWheelConfig conf){
	uint16_t val = (uint8_t)conf.enctype & 0x3f;
	val |= ((uint8_t)conf.drvtype & 0x3f) << 6;
	val |= (conf.axes & 0x03) << 12;
	return val;
}
FFBWheelAnalogConfig FFBWheel::decodeAnalogConfFromInt(uint16_t val){
	FFBWheelAnalogConfig aconf;
	aconf.analogmask = val & 0xff;
	aconf.offsetmode = AnalogOffset((val >> 8) & 0x3);
	return aconf;
}
uint16_t FFBWheel::encodeAnalogConfToInt(FFBWheelAnalogConfig conf){
	uint16_t val = conf.analogmask & 0xff;
	val |= (conf.analogmask & 0x3) << 8;
	return val;
}

