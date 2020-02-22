/*
 * FFBWheel.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#include "FFBWheel.h"
#include "FFBWheel_usb_init.h"
#include "voltagesense.h"

ClassIdentifier FFBWheel::info = {
		 .name = "FFB Wheel" ,
		 .id=1
 };

const ClassIdentifier FFBWheel::getInfo(){
	return info;
}

// Register possible button sources (id 0-15)
const std::vector<class_entry<ButtonSource>> button_sources =
{
		add_class<LocalButtons,ButtonSource>(),
		add_class<SPI_Buttons,ButtonSource>()
};
ClassChooser<ButtonSource> btn_chooser(button_sources);


FFBWheel::FFBWheel() {
	this->ffb = new HidFFB();
	// Setup a timer
	extern TIM_HandleTypeDef htim4;
	this->timer_update = &htim4; // Timer setup with prescaler of sysclock
	this->timer_update->Instance->ARR = 250;
	this->timer_update->Instance->CR1 = 1;
	HAL_TIM_Base_Start_IT(this->timer_update);

	// read all constants
	uint16_t confint;
	if(Flash_Read(ADR_FFBWHEEL_CONFIG, &confint)){
		this->conf = FFBWheel::decodeConfFromInt(confint);
	}else{
		pulseErrLed();
	}


	setDrvType(this->conf.drvtype);
	Flash_Read(ADR_FFBWHEEL_BUTTONCONF, &this->btnsources);
	setBtnTypes(this->btnsources);

	uint16_t ppr = 0;
	if(Flash_Read(ADR_TMC1_PPR, &ppr)){
		this->enc->setPpr(ppr);
	}else{
		pulseErrLed();
	}

	Flash_Read(ADR_FFBWHEEL_POWER, &this->power);

	uint16_t aconfint;
	if(Flash_Read(ADR_FFBWHEEL_ANALOGCONF,&aconfint)){
		this->aconf = FFBWheel::decodeAnalogConfFromInt(aconfint);
	}

	// Home

}

// Saves parameters to flash
void FFBWheel::saveFlash(){
	if(this->conf.drvtype == MotorDriverType::TMC4671_type){
		TMC4671* drv = static_cast<TMC4671*>(this->drv);
		//save motor config
		uint16_t motint = TMC4671::encodeMotToInt(drv->conf.motconf);
		Flash_Write(ADR_TMC1_POLES_MOTTYPE_PHIE, motint);


	}
	Flash_Write(ADR_FFBWHEEL_CONFIG,FFBWheel::encodeConfToInt(this->conf));
	Flash_Write(ADR_TMC1_PPR, enc->getPpr());
	Flash_Write(ADR_FFBWHEEL_POWER, this->power);
	Flash_Write(ADR_FFBWHEEL_BUTTONCONF,this->btnsources);
	Flash_Write(ADR_FFBWHEEL_ANALOGCONF, FFBWheel::encodeAnalogConfToInt(this->aconf));

}

void FFBWheel::update(){
	int16_t lasttorque = endstopTorque;
	bool updateTorque = false;

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

		if(this->conf.drvtype == MotorDriverType::TMC4671_type){
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
	int8_t clipval = cliptest<int32_t,int32_t>(lastScaledEnc, -0x7fff, 0x7fff);
	if(clipval == 0){
		return 0;
	}
	int32_t addtorque = 0;

	addtorque += clip<int32_t,int32_t>(abs(lastScaledEnc)-0x7fff,-0x7fff,0x7fff);
	addtorque *= 40;
	addtorque *= -clipval;


	return clip<int32_t,int32_t>(addtorque,-0x7fff,0x7fff);
}



void FFBWheel::setupTMC4671(){
	this->conf.drvtype = MotorDriverType::TMC4671_type;
	TMC4671MainConfig tmcconf;
	uint16_t mconfint;
	TMC4671MotConf motconf;
	if(Flash_Read(ADR_TMC1_POLES_MOTTYPE_PHIE, &mconfint)){
		motconf = TMC4671::decodeMotFromInt(mconfint);
	}

	tmcconf.motconf = motconf;
	this->drv = new TMC4671(&HSPIDRV,SPI1_SS1_GPIO_Port,SPI1_SS1_Pin,tmcconf);
	TMC4671* drv = static_cast<TMC4671*>(this->drv);
	drv->setPids(tmcpids);
	if(tmcFeedForward){
		drv->setupFeedForwardTorque(torqueFFgain, torqueFFconst);
		drv->setupFeedForwardVelocity(velocityFFgain, velocityFFconst);
		drv->setFFMode(FFMode::torque);
	}


	drv->initialize();

	setupTMC4671_enc(this->conf.enctype);

	drv->setMotionMode(MotionMode::torque);

}
FFBWheelConfig FFBWheel::decodeConfFromInt(uint16_t val){
	// 0-2 ENC, 3-5 DRV, 6-8 BTN
	FFBWheelConfig conf;
	conf.enctype = EncoderType((val) & 0x07);
	conf.drvtype = MotorDriverType((val >> 3) & 0x07);

	return conf;
}
uint16_t FFBWheel::encodeConfToInt(FFBWheelConfig conf){
	uint16_t val = (uint8_t)conf.enctype & 0x7;
	val |= ((uint8_t)conf.drvtype & 0x7) << 3;

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



void FFBWheel::setupTMC4671_enc(EncoderType enctype){
	this->conf.enctype = enctype;
	TMC4671* drv = static_cast<TMC4671*>(this->drv);
	this->enc = static_cast<Encoder*>(drv);

	//TODO
	if(this->conf.enctype == EncoderType::ABN_TMC){
		uint16_t ppr = 0;
		Flash_Read(ADR_TMC1_PPR, &ppr);
		TMC4671ABNConf encconf;
		encconf.ppr = ppr;
		bool npol = drv->findABNPol();
		encconf.apol = npol;
		encconf.bpol = npol;
		encconf.npol = npol;
		drv->setup_ABN_Enc(encconf);
		drv->setPhiEtype(PhiE::abn);

	}else if(this->conf.enctype == EncoderType::HALL_TMC){
		TMC4671HALLConf hallconf;
		drv->setup_HALL(hallconf);
		drv->setPhiEtype(PhiE::hall);
	}
}

void FFBWheel::setDrvType(MotorDriverType drvtype){
	if(drvtype < MotorDriverType::NONE){
		this->conf.drvtype = drvtype;
	}
	if(this->conf.drvtype == MotorDriverType::TMC4671_type){
		setupTMC4671();
	}
}

void FFBWheel::setEncType(EncoderType enctype){
	if(this->conf.drvtype == MotorDriverType::TMC4671_type){
		setupTMC4671_enc(enctype);
	}
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

FFBWheel::~FFBWheel() {

}

bool FFBWheel::executeUserCommand(ParsedCommand* cmd,std::string* reply){
	bool flag = true;
	// ------------ General commands ----------------
	if(cmd->cmd == "save"){
		this->saveFlash();
		*reply+=">saved";
	}else if(cmd->cmd == "drvtype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.drvtype);
		}else if(cmd->type == CMDtype::set && cmd->val < (uint8_t)MotorDriverType::NONE){
			setDrvType(MotorDriverType(cmd->val));
		}else{
			*reply += "0:TMC4671";
		}
	}else if(cmd->cmd == "btntypes"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->btnsources);
		}else if(cmd->type == CMDtype::set){
			setBtnTypes(cmd->val);
		}else{
			*reply += "bin flag encoded list of button sources. (3 = source 1 & 2 active). See lsbtn for sources";
		}
	}else if(cmd->cmd == "lsbtn"){
		if(cmd->type == CMDtype::get){
			*reply += btn_chooser.printAvailableClasses();
		}
	}else if(cmd->cmd == "addbtn"){
		if(cmd->type == CMDtype::set){
			this->addBtnType(cmd->val);
		}
	}else if(cmd->cmd == "numspibuttons"){
		if(cmd->type == CMDtype::set){
			SPI_Buttons::writeConfNumButtons(cmd->val);
			this->setBtnTypes(this->btnsources); // Reload sources
		}else if(cmd->type == CMDtype::get){
			*reply+=std::to_string(SPI_Buttons::readConfNumButtons());
		}else{
			*reply+="Read or store global spi button number. Negative to cut left side";
		}

	}else if(cmd->cmd == "power"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(power);
		}else if(cmd->type == CMDtype::set){
			this->power = cmd->val;
		}
	}else if(cmd->cmd == "degrees"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->degreesOfRotation);
		}else if(cmd->type == CMDtype::set){
			this->degreesOfRotation = cmd->val;
		}
	}else if(cmd->cmd == "enctype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.enctype);
		}else if(cmd->type == CMDtype::set && cmd->val < (uint8_t)EncoderType::NONE){
			setEncType(EncoderType(cmd->val));
		}else{
			*reply += "0:ABN_LOCAL\nA1:BN_TMC\n2:HALL_TMC";
		}
	}else if(cmd->cmd == "axismask"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(aconf.analogmask);
		}else if(cmd->type == CMDtype::set){
			aconf.analogmask = cmd->val;
		}
	}else if(cmd->cmd == "ppr"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->enc->getPpr());
		}else if(cmd->type == CMDtype::set && this->enc != nullptr){
			this->enc->setPpr(cmd->val);
		}else{
			*reply += "Err. Setup enctype first";
		}
	}else if(cmd->cmd == "pos"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->enc->getPos());
		}else if(cmd->type == CMDtype::set && this->enc != nullptr){
			this->enc->setPos(cmd->val);
		}else{
			*reply += "Err. Setup enctype first";
		}

	}else if(cmd->cmd == "hidrate" && cmd->type == CMDtype::get){
		*reply+=std::to_string(1000/ffb->hid_out_period);

	}else if(cmd->cmd == "help"){
		*reply += "<HELP>"; // TODO
	}

	// ----------- TMC 4671 specific commands-------------
	if(this->conf.drvtype == MotorDriverType::TMC4671_type){
		TMC4671* drv = static_cast<TMC4671*>(this->drv);
		if(cmd->cmd == "mtype"){
			if(cmd->type == CMDtype::get){
				*reply+=std::to_string((uint8_t)drv->conf.motconf.motor_type);
			}else if(cmd->type == CMDtype::set && (uint8_t)cmd->type < (uint8_t)MotorType::ERR){
				drv->setMotorType((MotorType)cmd->val, drv->conf.motconf.pole_pairs);
			}

		}else if(cmd->cmd == "encalign"){
			if(cmd->type == CMDtype::get){
				drv->bangInitABN(3000);
			}else if(cmd->type ==CMDtype:: set){
				drv->bangInitABN(cmd->val);
			}
		}else if(cmd->cmd == "poles"){
			if(cmd->type == CMDtype::get){
				*reply+=std::to_string(drv->conf.motconf.pole_pairs);
			}else if(cmd->type == CMDtype::set){
				drv->setMotorType(drv->conf.motconf.motor_type,cmd->val);
			}

		}else if(cmd->cmd == "phiesrc"){
			if(cmd->type == CMDtype::get){
				*reply+=std::to_string((uint8_t)drv->conf.motconf.phiEsource);
			}else if(cmd->type == CMDtype::set){
				drv->setPhiEtype((PhiE)cmd->val);
			}

		}else if(cmd->cmd == "reg"){
			if(cmd->type == CMDtype::getat){
				*reply+=std::to_string(drv->readReg(cmd->val));
			}else if(cmd->type == CMDtype::setat){
				drv->writeReg(cmd->adr,cmd->val);
			}

		}else{
			flag=false;
		}
	}

	return flag;
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
	//btn->readButtons(&reportHID.buttons);

	// Encoder
	reportHID.X = clip(lastScaledEnc,-0x7fff,0x7fff);
	// Analog values read by DMA
	uint16_t analogMask = this->aconf.analogmask;
	reportHID.Y 	=  (analogMask & 0x01) ? ((adc_buf[0] & 0xFFF) << 4)-0x7fff : 0;
	reportHID.Z		=  (analogMask & 0x01<<1) ? ((adc_buf[1] & 0xFFF) << 4)-0x7fff : 0;
	reportHID.RX	=  (analogMask & 0x01<<2) ? ((adc_buf[2] & 0xFFF) << 4)-0x7fff : 0;
	reportHID.RY	=  (analogMask & 0x01<<3) ? ((adc_buf[3] & 0xFFF) << 4)-0x7fff : 0;
	reportHID.RZ	=  (analogMask & 0x01<<4) ? ((adc_buf[4] & 0xFFF) << 4)-0x7fff : 0;
	reportHID.Slider=  (analogMask & 0x01<<5) ? ((adc_buf[5] & 0xFFF) << 4)-0x7fff : 0;

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
