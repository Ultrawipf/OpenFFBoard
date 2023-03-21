/*
 * FFBShifter.cpp
 *
 *  Created on: 02.03.2023
 *      Author: Yannick
 */

#include "FFBShifter.h"
#include "math.h"
#ifdef FFBSHIFTER

// Unique identifier for listing
ClassIdentifier FFBShifter::info = {
		 .name = "FFB Shifter" ,
		 .id=CLSID_MAIN_FFBSHIFTER,
 };

const ClassIdentifier FFBShifter::getInfo(){
	return info;
}

const std::array<char*,2> modenames = {"Sequential","H-Pattern"};

FFBShifter::FFBShifter() : FFBHIDMain(2){
	FFBHIDMain::setFFBEffectsCalc(ffb, static_cast<std::shared_ptr<EffectsCalculatorItf>>(effects_calc));

}

FFBShifter::~FFBShifter() {
	// TODO Auto-generated destructor stub
}

void FFBShifter::usbInit(){
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_hid_conf_gamepad,&usb_ffboard_strings_default);
	FFBHIDMain::UsbHidHandler::setHidDesc(hid_gamepad_desc);

	usbdev->registerUsb();
}

/**
 * Read the shifter buttons
 */
uint8_t FFBShifter::readInternalButtons(uint64_t* btn){
	ButtonSource* btnsrc = static_cast<ButtonSource*>(effects_calc.get());
	return btnsrc->readButtons(btn);
}

// Shifter effects. Create the snappy positions here

FFBShifterEffects::FFBShifterEffects():CommandHandler("shifterfx",CLSID_FFBSHIFTER_FX,0) {


	CommandHandler::registerCommands();
	registerCommand("active", FFBShifterEffect_commands::active, "Enable/Disable",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("invert", FFBShifterEffect_commands::invert, "Flip X/Y axes",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("mode", FFBShifterEffect_commands::mode, "Pattern mode",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
//	registerCommand("polarity", LocalButtons_commands::polarity, "Pin polarity",CMDFLAG_GET | CMDFLAG_SET);
//	registerCommand("pins", LocalButtons_commands::pins, "Available pins",CMDFLAG_GET | CMDFLAG_SET);
//	registerCommand("values", LocalButtons_commands::values, "pin values",CMDFLAG_GET);
	setMode(this->mode); // Confirm mode
}

FFBShifterEffects::~FFBShifterEffects(){

}

ClassIdentifier FFBShifterEffects::info = {
	 .name = "FFB Shifter FX" ,
	 .id=CLSID_FFBSHIFTER_FX,
};
const ClassIdentifier FFBShifterEffects::getInfo(){
	return info;
}


void FFBShifterEffects::setActive(bool active){
	this->active = active;
}

/**
 * A general spring effect
 */
int32_t FFBShifterEffects::springEffect(int32_t position, int32_t offset,float coefficient,int32_t negativeSaturation,int32_t positiveSaturation,int32_t deadBand){

	int32_t force = 0;

	// Effect is only active outside deadband + offset
	if (fabsf(position - offset) > deadBand){

		// remove offset/deadband from metric to compute force
		position = position - (offset + (deadBand * (position < offset ? -1 : 1)) );

		force = clip<int32_t, int32_t>((coefficient * (float)(position)),
										-negativeSaturation,
										 positiveSaturation);
	}

	return force;
}

void FFBShifterEffects::calculateShifterEffect(metric_t* metricsX,metric_t* metricsY, int32_t* torqueX,int32_t* torqueY){
	if(!metricsX || !metricsY){
		return;
	}
	int32_t posY = metricsY->pos;
	int32_t posX = metricsX->pos;
	float pos_fY = metricsY->pos_f;
	float pos_fX = metricsX->pos_f;
	float rangeY = params.range;
	float rangeX = params.rangeX;
	switch(mode){
		case FFBShifterEffectMode::sequential:
		{
			// X axis
			*torqueX = springEffect(posX, 0, params.seqgainX);
			// Y axis
			if(fabsf(pos_fY) < rangeY * 0.5){ // Center point
				buttons = 0;
				*torqueY += springEffect(posY, 0, params.gainYseq, params.maxForceY, params.maxForceY, 0);
			}else if(pos_fY > 0){ // Upper section
				// Button 0
				buttons = 1;
				*torqueY += springEffect(posY, 0x7fff * rangeY * params.seqsnappoint, params.seqsnap);
			}else if(pos_fY < 0){ // Lower section
				// Button 1
				buttons = 2;
				*torqueY += springEffect(posY, -0x7fff * rangeY * params.seqsnappoint, params.seqsnap);
			}



			break;
		}
		// TODO h pattern with different reverse positions
		case FFBShifterEffectMode::h_sym:
		{
			uint8_t buttonSection = 0;
			// Makes 3 stable snap points in Y direction
			if(fabsf(pos_fY) < rangeY * 0.5){ // Center point
				*torqueY += springEffect(posY, 0, params.gainY, params.maxForceY, params.maxForceY, 0);
			}else if(pos_fY > 0){ // Upper section
				buttonSection = 1;
				*torqueY += springEffect(posY, 0x7fff * rangeY * params.gateBegin, params.gainY, params.maxForceY, params.maxForceY, 0);
			}else if(pos_fY < 0){ // Lower section
				buttonSection = 2;
				*torqueY += springEffect(posY, -0x7fff * rangeY * params.gateBegin, params.gainY, params.maxForceY, params.maxForceY, 0);
			}

			// X dir
			if(params.hgatesX == 0 || params.hgatesX > 15){
				break;
			}
			buttons = 0;
			float gatesize = 2.0/(params.hgatesX);

			if(abs(pos_fY) < rangeY * 0.4){ // Center before gates in Y direction
				// Nothing. Just weak centering
				*torqueX += springEffect(posX, 0, params.gainX, params.maxCenterForceX, params.maxCenterForceX, 0);

			}else{
				// Generate some gates in X direction

				//*torqueX += springEffect(posX, (posX % (0x7fff / params.hgatesX)), params.gainXgate, params.maxForceX, params.maxForceX, 0);
				// Depending on the number of gates make a symmetrical H Pattern.
				// float pos -1 to 1


				for(uint8_t gate = 0;gate<params.hgatesX;gate++){
					if((gate == 0 || pos_fX > (gate * gatesize)-1.0) && (pos_fX < (gatesize * (gate+1)) -1.0 || gate == params.hgatesX-1) ){
						if(buttonSection){
							uint32_t btn = (gate * 2) + buttonSection - 1;
							buttons |= (1 << btn);
						}

						int32_t gateTorque = springEffect(posX, 0x7fff * ((gatesize * (gate+0.5))-1), params.gainXgate, params.maxForceX, params.maxForceX, 0);
						float fadeVal = clip<float,float>((abs(pos_fY)-params.gateBegin) * params.gateFade, 0, 1);
						*torqueX += gateTorque * fadeVal;
					}
				}

			}


			*torqueY += springEffect(posY, 0, 10, params.maxForceY, params.maxForceX, 0x7fff * rangeY); // Endstop
			uint32_t deadzone_X = (gatesize * (params.hgatesX + 0.5) -1.0) * 0x3fff;
			*torqueX += springEffect(posX, 0, 10, params.maxForceX, params.maxForceX, deadzone_X); // Endstop if no gate
		}
			break;
	}
}


void FFBShifterEffects::calculateEffects(std::vector<std::unique_ptr<Axis>> &axes){
	// Will have 2 axes created via the FFBHIDMain
	metric_t *metricsX = 0, *metricsY = 0;
	uint8_t idX = invertAxes ? 1 : 0;
	uint8_t idY = invertAxes ? 0 : 1;

	// Get metrics of both axes
	for (auto &axis : axes){
		axis->calculateAxisEffects(active); // Damper, idle spring
		metric_t* metrics = axis->getMetrics();
		uint8_t dir = axis->getCommandHandlerInstance();
		if(dir == idX){
			metricsX = metrics;
		}else if(dir == idY){
			metricsY = metrics;
		}

	}

	int32_t torqueX = 0, torqueY = 0;
	// Calculate forces
	calculateShifterEffect(metricsX, metricsY, &torqueX, &torqueY);
	// Send out forces to axes
	for (auto &axis : axes){
		uint8_t dir = axis->getCommandHandlerInstance();
		if(dir == idX){
			axis->setEffectTorque(-torqueX); // Negative
		}else if(dir == idY){
			axis->setEffectTorque(-torqueY);
		}

	}

}


void FFBShifterEffects::setMode(FFBShifterEffectMode mode){
	if(mode == FFBShifterEffectMode::sequential){
		ButtonSource::btnnum = 2;
	}
	this->mode = mode;
}
void FFBShifterEffects::setMode_i(const uint8_t mode){
	setMode(static_cast<FFBShifterEffectMode>(mode));
}

uint8_t FFBShifterEffects::readButtons(uint64_t* buf){
	*buf |= this->buttons;
	return ButtonSource::btnnum;
}


CommandStatus FFBShifterEffects::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<FFBShifterEffect_commands>(cmd.cmdId)){
	case FFBShifterEffect_commands::active:
		return handleGetSet(cmd, replies, this->active);
	break;

	case FFBShifterEffect_commands::invert:
		return handleGetSet(cmd, replies, this->invertAxes);
	break;
	case FFBShifterEffect_commands::mode:
		if(cmd.type == CMDtype::info){
			for(uint8_t i = 0; i<modenames.size();i++){
				replies.emplace_back(std::string(modenames[i])  + ":" + std::to_string(i)+"\n");
			}
		}else{
			return handleGetSetFunc(cmd, replies, (uint8_t&)this->mode, &FFBShifterEffects::setMode_i, this);
		}
	break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return CommandStatus::OK;

}

#endif
