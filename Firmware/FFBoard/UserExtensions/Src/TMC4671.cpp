/*
 * TMC4671.cpp
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#include "TMC4671.h"
#include "ledEffects.h"
#include "voltagesense.h"
#include "stm32f4xx_hal_spi.h"
#include <math.h>
#include <assert.h>
#include "RessourceManager.h"
#include "ErrorHandler.h"
#include "cpp_target_config.h"
#define MAX_TMC_DRIVERS 3

ClassIdentifier TMC_1::info = {
	.name = "TMC4671_1" ,
	.id=1,
	.unique = 'X'
};




bool TMC_1::isCreatable() {
	return motor_spi.isPinFree(OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin));
}


ClassIdentifier TMC_2::info = {
	.name = "TMC4671_2" ,
	.id=2,
	.unique = 'Y'
};




bool TMC_2::isCreatable() {
	return motor_spi.isPinFree(OutputPin(*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin));
}




ClassIdentifier TMC4671::info = {
	.name = "TMC4671" ,
	.id=3,
	.unique = '0'
};
//const ClassIdentifier TMC4671::getInfo(){
//	return info;
//}

//TMC4671::TMC4671(SPI_HandleTypeDef* spi,OutputPin cspin,TMC4671MainConfig conf) : Thread("TMC", TMC_THREAD_MEM, TMC_THREAD_PRIO){
////	this->cspin = cspin;
////	this->csport = csport;
////	this->spi = spi;
//	this->conf = conf;
//	spi_config.cs = cspin;
//	this->restoreFlash();
//}

TMC4671::TMC4671(SPIPort& spiport,OutputPin cspin,uint8_t address) : SPIDevice{motor_spi,cspin},Thread("TMC", TMC_THREAD_MEM, TMC_THREAD_PRIO){
	setAddress(address);
//	this->cspin = SPI1_SS1_Pin;
//	this->csport = SPI1_SS1_GPIO_Port;
//	this->spi = &HSPIDRV;

	spiConfig.peripheral.Mode = SPI_MODE_MASTER;
	spiConfig.peripheral.Direction = SPI_DIRECTION_2LINES;
	spiConfig.peripheral.DataSize = SPI_DATASIZE_8BIT;
	spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
	spiConfig.peripheral.NSS = SPI_NSS_SOFT;
	spiConfig.peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
	spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	spiConfig.peripheral.TIMode = SPI_TIMODE_DISABLE;
	spiConfig.peripheral.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	spiConfig.peripheral.CRCPolynomial = 10;
	spiConfig.cspol = true;

	spiPort.takeSemaphore();
	spiPort.configurePort(&spiConfig.peripheral);
	spiPort.giveSemaphore();

	//attachToPort(motor_spi);
	this->restoreFlash();
}


TMC4671::~TMC4671() {
	enablePin.reset();
	//recordSpiAddrUsed(0);
}


const ClassIdentifier TMC4671::getInfo() {
//	char axis_letter = '0' + this->axis;
//	std::string axis_name = "TMC4671-";
//	axis_name.push_back(axis_letter);
//	return ClassIdentifier {.name = axis_name.c_str(), .id = 1, .instance = this->axis, .hidden = false};
	return ClassIdentifier {.name = TMC4671::info.name, .id = TMC4671::info.id, .unique = this->axis, .hidden = TMC4671::info.hidden};
}


void TMC4671::setAddress(uint8_t address){
	if (address == 1){
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC1_MOTCONF, ADR_TMC1_CPR, ADR_TMC1_ENCA, ADR_TMC1_OFFSETFLUX, ADR_TMC1_TORQUE_P, ADR_TMC1_TORQUE_I, ADR_TMC1_FLUX_P, ADR_TMC1_FLUX_I});
	}else if (address == 2)
	{
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC2_MOTCONF, ADR_TMC2_CPR, ADR_TMC2_ENCA, ADR_TMC2_OFFSETFLUX, ADR_TMC2_TORQUE_P, ADR_TMC2_TORQUE_I, ADR_TMC2_FLUX_P, ADR_TMC2_FLUX_I});
	}else if (address == 3)
	{
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC3_MOTCONF, ADR_TMC3_CPR, ADR_TMC3_ENCA, ADR_TMC3_OFFSETFLUX, ADR_TMC3_TORQUE_P, ADR_TMC3_TORQUE_I, ADR_TMC3_FLUX_P, ADR_TMC3_FLUX_I});
	}
	//this->setAxis((char)('W'+address));
}


void TMC4671::setAxis(char axis){
	this->axis = axis;
	this->info.unique = axis;
}

uint8_t TMC4671::getAxis(){
	return this->axis;
}


void TMC4671::saveFlash(){
	uint16_t mconfint = TMC4671::encodeMotToInt(this->conf.motconf);
	uint16_t abncpr = this->conf.motconf.enctype == EncoderType_TMC::abn ? this->abnconf.cpr : this->aencconf.cpr;
	// Save flash
	Flash_Write(flashAddrs.mconf, mconfint);
	Flash_Write(flashAddrs.cpr, abncpr);
	Flash_Write(flashAddrs.offsetFlux,maxOffsetFlux);
	Flash_Write(flashAddrs.encA,encodeEncHallMisc());

	Flash_Write(flashAddrs.torque_p, curPids.torqueP);
	Flash_Write(flashAddrs.torque_i, curPids.torqueI);
	Flash_Write(flashAddrs.flux_p, curPids.fluxP);
	Flash_Write(flashAddrs.flux_i, curPids.fluxI);
}

void TMC4671::restoreFlash(){
	uint16_t mconfint;
	uint16_t abncpr = 0;

	// Read flash
	if(Flash_Read(flashAddrs.mconf, &mconfint))
		this->conf.motconf = TMC4671::decodeMotFromInt(mconfint);

	if(Flash_Read(flashAddrs.cpr, &abncpr))
		setCpr(abncpr);

	// Pids
	Flash_Read(flashAddrs.torque_p, &this->curPids.torqueP);
	Flash_Read(flashAddrs.torque_i, &this->curPids.torqueI);
	Flash_Read(flashAddrs.flux_p, &this->curPids.fluxP);
	Flash_Read(flashAddrs.flux_i, &this->curPids.fluxI);

	Flash_Read(flashAddrs.offsetFlux, (uint16_t*)&this->maxOffsetFlux);

	uint16_t miscval;
	if(Flash_Read(flashAddrs.encA, &miscval)){
		restoreEncHallMisc(miscval);
	}

	setPids(curPids); // Write pid values to tmc
}

bool TMC4671::hasPower(){
	uint16_t intV = getIntV();
	return (intV > 10000) && (getExtV() > 10000) && (intV < 78000);
}

// Checks if important parameters are set to valid values
bool TMC4671::isSetUp(){

	if(this->conf.motconf.motor_type == MotorType::NONE){
		return false;
	}

	// Encoder
	if(this->conf.motconf.phiEsource == PhiE::abn){
		if(abnconf.cpr == 0){
			return false;
		}
		if(this->encstate != ENC_InitState::OK){
			return false;
		}
	}

	return true;
}

/*
 * Check if driver is responding
 */
bool TMC4671::pingDriver(){
	writeReg(1, 0);
	return(readReg(0) == 0x34363731);
}


/*
 * Sets all parameters of the driver at startup
 */
bool TMC4671::initialize(){
//	active = true;
//	if(state == TMC_ControlState::uninitialized){
//		state = TMC_ControlState::Init_wait;
//	}
	// Check if a TMC4671 is active and replies correctly
	if(!pingDriver()){
		ErrorHandler::addError(Error(ErrorCode::tmcCommunicationError, ErrorType::warning, std::string(this->getInfo().name) + " " + this->getInfo().unique + " not responding"));
		return false;
	}

	writeReg(1, 1);
	if(readReg(0) == 0x00010000 && allowSlowSPI){
		/* Slow down SPI if old TMC engineering sample is detected
		 * The first version has a high chance of glitches of the MSB
		 * when high spi speeds are used.
		 * This can cause problems for some operations.
		 */
		pulseClipLed();

		this->spiConfig.peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
		spiPort.configurePort(&this->spiConfig.peripheral);
		oldTMCdetected = true;
	}

	if(!oldTMCdetected){
		this->setPidPrecision(pidPrecision);
	}

	// Write main constants

	writeReg(0x64, 0); // No flux/torque
	setPwm(0,conf.pwmcnt,conf.bbmL,conf.bbmH); // Set FOC @ 25khz but turn off pwm for now
	setMotorType(conf.motconf.motor_type,conf.motconf.pole_pairs);
	setPhiEtype(conf.motconf.phiEsource);
	setup_HALL(hallconf); // Enables hall filter and masking

	initAdc(conf.mdecA,conf.mdecB,conf.mclkA,conf.mclkB);
	setAdcOffset(conf.adc_I0_offset, conf.adc_I1_offset);
	setAdcScale(conf.adc_I0_scale, conf.adc_I1_scale);

	// Initial adc calibration
	if(!calibrateAdcOffset(150)){
		changeState(TMC_ControlState::HardError); // ADC or shunt amp is broken!
		enablePin.reset();
		return false;
	}
	// brake res failsafe.
	/*
	 * Single ended input raw value
	 * 0V = 0x7fff
	 * 4.7k / (360k+4.7k) Divider on old board.
	 * 1.5k / (71.5k+1.5k) 16.121 counts 60V new. 100V VM => 2V
	 * 13106 counts/V input.
	 */
	if(oldTMCdetected){
		setBrakeLimits(52400,52800);
	}else{
		setBrakeLimits(50700,50900); // Activates around 60V as last resort failsave. Check offsets from tmc leakage
	}

	// Status mask
	if(oldTMCdetected){
		setStatusMask(0); // ES Version status output is broken
	}else{
		statusMask.asInt = 0;
		statusMask.flags.adc_i_clipped = 1;
		statusMask.flags.not_PLL_locked = 1;
		setStatusMask(statusMask);
	}

	setPids(curPids); // Write basic pids

	if(hasPower()){
		enablePin.set();
		setPwm(7);
		calibrateAdcOffset(400); // Calibrate ADC again with power
		active = true;
	}
	setEncoderType(conf.motconf.enctype);

	// Update flags
	readFlags(false); // Read all flags
	// Home?
	// Run in direction of N pulse. Enable flag/interrupt
	//runOpenLoop(3000, 0, 5, 100);
	initialized = true;
	initTime = HAL_GetTick();
	return initialized;
}

/*
 * Not calibrated perfectly!
 */
float TMC4671::getTemp(){
	writeReg(0x03, 2);
	int32_t adcval = ((readReg(0x02)) & 0xffff) - 0x7fff; // Center offset
	adcval -= tmcOffset;
	if(adcval == 0){
		return 0.0;
	}
	float r = thermistor_R2 * (((float)43252 / (float)adcval)); //43252 equivalent ADC count if it was 3.3V and not 2.5V

	// Beta
	r = (1.0 / 298.15) + log(r / thermistor_R) / thermistor_Beta;
	r = 1.0 / r;
	r -= 273.15;
	return r;

}

bool TMC4671::motorReady(){
	return this->state == TMC_ControlState::Running;
}

void TMC4671::Run(){
	// Main state machine
	while(1){

		switch(this->state){

		case TMC_ControlState::Running:
		{
			// Check status, Temps, Everything alright?
			uint32_t tick = HAL_GetTick();
			if(tick - lastStatTime > 2000){ // Every 2s
				lastStatTime = tick;
				statusCheck();
				// Get enable input
				bool tmc_en = (readReg(0x76) >> 15) & 0x01;
				if(!tmc_en && active){ // Hardware emergency. If tmc does not reply the enable will still read false
					this->estopTriggered = true;
					this->emergencyStop();
					changeState(TMC_ControlState::HardError);
				}

				// Temperature sense
				#ifdef TMCTEMP
				float temp = getTemp();
				if(temp > temp_limit){
					changeState(TMC_ControlState::OverTemp);
					pulseErrLed();
				}
				#endif

			}
			Delay(200);
		}
		break;

		case TMC_ControlState::Init_wait:
			if(active && hasPower()){
				if(HAL_GetTick() - initTime > (emergency ? 5000 : 1000)){
					emergency = false;
					if(!initialize()){
						pulseErrLed();
					}
				}
			}

		break;

		case TMC_ControlState::ABN_init:
			ABN_init();
		break;

		case TMC_ControlState::AENC_init:
			AENC_init();
		break;

		case TMC_ControlState::HardError:

		break; // Broken

		case TMC_ControlState::OverTemp:
			this->stopMotor();
			changeState(TMC_ControlState::HardError); // Block
		break;

		case TMC_ControlState::EncoderFinished: // Startup sequence done
			setEncoderIndexFlagEnabled(true); // TODO
			if(active){
				startMotor();
				changeState(TMC_ControlState::Running);
			}else{
				stopMotor();
				laststate = TMC_ControlState::Running; // Go to running when starting again
			}
		break;

		case TMC_ControlState::No_power:
			if(hasPower() && !emergency){
				changeState(laststateNopower);
				setMotionMode(lastMotionMode,true);
				ErrorHandler::clearError(ErrorCode::undervoltage);
				enablePin.set();
			}
			pulseErrLed();
			Delay(100); // wait a bit more
		break;

		default:

		break;
		}

		// Optional update methods for safety

		if(!hasPower() && state != TMC_ControlState::No_power){ // low voltage or overvoltage
			lastMotionMode = curMotionMode;
			laststateNopower = state;
			ErrorHandler::addError(lowVoltageError);
			setMotionMode(MotionMode::stop,true); // Disable tmc
			changeState(TMC_ControlState::No_power);
			enablePin.reset();
		}
		Delay(10);
	} // End while
}

/*
 * Returns the current state of the driver controller
 */
TMC_ControlState TMC4671::getState(){
	return this->state;
}

inline void TMC4671::changeState(TMC_ControlState newState){
	if(newState != this->state){
		this->laststate = this->state; // save last state if new state wants to jump back
	}
	this->state = newState;
}

bool TMC4671::reachedPosition(uint16_t tolerance){
	int32_t actualPos = readReg(0x6B);
	int32_t targetPos = readReg(0x68);
	if( abs(targetPos - actualPos) < tolerance){
		return true;
	}else{
		return false;
	}
}

void TMC4671::setEncoderIndexFlagEnabled(bool enabled){
	this->statusMask.flags.AENC_N = this->conf.motconf.enctype == EncoderType_TMC::sincos && enabled;
	this->statusMask.flags.ENC_N = this->conf.motconf.enctype == EncoderType_TMC::abn && enabled;
	setStatusMask(statusMask); // Enable flag output for encoder
}

void TMC4671::setTargetPos(int32_t pos){
	if(curMotionMode != MotionMode::position){
		setMotionMode(MotionMode::position,true);
	}
	writeReg(0x68,pos);
}
int32_t TMC4671::getTargetPos(){

	return readReg(0x68);
}


void TMC4671::setTargetVelocity(int32_t vel){
	if(curMotionMode != MotionMode::velocity){
		setMotionMode(MotionMode::velocity,true);
	}
	writeReg(0x66,vel);
}
int32_t TMC4671::getTargetVelocity(){
	return readReg(0x66);
}
int32_t TMC4671::getVelocity(){
	return readReg(0x6A);
}

void TMC4671::setPositionExt(int32_t pos){
	writeReg(0x1E, pos);
}

void TMC4671::setPhiE_ext(int16_t phiE){
	writeReg(0x1C, phiE);
}

/*
 * Aligns ABN encoders by forcing an angle with high current and calculating the offset
 */
void TMC4671::bangInitEnc(int16_t power){
	if(!hasPower() || (this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC)){ // If not stepper or bldc return
		return;
	}
	blinkClipLed(50, 0);
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setPhiE_ext(0);
	setPhiEtype(PhiE::ext);
	//setFluxTorque(power, 0);
	//int32_t pos = getPos();



	uint8_t phiEreg = 0;
	uint8_t phiEoffsetReg = 0;
	if(conf.motconf.enctype == EncoderType_TMC::abn){
		phiEreg = 0x2A;
		phiEoffsetReg = 0x29;
		writeReg(0x27,0); //Zero encoder
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		phiEreg = 0x46;
		writeReg(0x41,0); //Zero encoder
		writeReg(0x47,0); //Zero encoder
		phiEoffsetReg = 0x45;
	}

	setPos(0);
	updateReg(phiEoffsetReg, 0, 0xffff, 16); // Set phiE offset to zero
	//setMotionMode(MotionMode::uqudext);

	//Delay(100);
	int16_t phiEpos = 0; // This is where the check starts too
	setPhiE_ext(phiEpos);
	// Ramp up flux
	for(int16_t flux = 0; flux <= power; flux+=10){
		setFluxTorque(flux, 0);
		Delay(3);
	}

	int16_t phiE_enc = readReg(phiEreg)>>16;
	Delay(100);
	int16_t phiE_abn_old = 0;
	int16_t c = 0;
	uint16_t still = 0;
	while(still < 30 && c++ < 4000){
		// Wait for motor to stop moving
		if(abs(phiE_enc - phiE_abn_old) < 200){
			still++;
		}else{
			still = 0;
		}
		phiE_abn_old = phiE_enc;
		phiE_enc=readReg(phiEreg)>>16;
		Delay(10);
	}

	//Write offset
	//int16_t phiE_abn = readReg(0x2A)>>16;
	abnconf.phiEoffset = phiEpos-phiE_enc;
	updateReg(phiEoffsetReg, abnconf.phiEoffset, 0xffff, 16);

	setFluxTorque(0, 0);
	setPhiE_ext(0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);
	//setPos(pos+getPos());
	setPos(0);

	blinkClipLed(0, 0);
}

// Rotates motor to find min and max values of the encoder
void TMC4671::calibrateAenc(){


	// static offset estimation. not exact
//	blinkClipLed(250, 0);
//	uint32_t offset = 0;
//	uint32_t samples = 0;
//	for(uint16_t i = 0;i<500;i++){
//		Delay(1);
//		writeReg(0x03,2);
//		uint32_t aencUX = readReg(0x02)>>16;
//		aencUX *= aencUX;
//		writeReg(0x03,3);
//		uint32_t aencWY_VN = readReg(0x02) ;
//		uint32_t aencWY = aencWY_VN >> 16;
//		aencWY *= aencWY;
//		uint32_t aencVN = aencWY_VN & 0xffff;
//		aencVN *= aencVN;
//
//		offset += std::sqrt((aencUX+aencWY) / 2);
//		samples++;
//	}
//	offset = offset/samples;
//	aencconf.AENC0_offset = offset;
//	aencconf.AENC1_offset = offset;
//	aencconf.AENC2_offset = offset;
//	setup_AENC(aencconf);
//	blinkClipLed(0, 0);


	// Rotate and measure min/max
	blinkClipLed(250, 0);
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	//int32_t pos = getPos();
	PosSelection possel = this->conf.motconf.pos_sel;
	setPosSel(PosSelection::PhiE_openloop);
	setPos(0);
	// Ramp up flux
	setFluxTorque(0, 0);
	writeReg(0x23,0); // set phie openloop 0
	setPhiEtype(PhiE::openloop);
	setMotionMode(MotionMode::torque,true);

	if(this->conf.motconf.motor_type == MotorType::STEPPER || this->conf.motconf.motor_type == MotorType::BLDC)
		for(int16_t flux = 0; flux <= bangInitPower; flux+=10){
			setFluxTorque(flux, 0);
			Delay(10);
		}

	uint32_t minVal_0 = 0xffff,	minVal_1 = 0xffff,	minVal_2 = 0xffff;
	uint32_t maxVal_0 = 0,	maxVal_1 = 0,	maxVal_2 = 0;
	int32_t minpos = -0x8fff/std::max<int32_t>(1,std::min<int32_t>(this->aencconf.cpr/4,20)), maxpos = 0x8fff/std::max<int32_t>(1,std::min<int32_t>(this->aencconf.cpr/4,20));
	uint32_t speed = std::max<uint32_t>(1,20/std::max<uint32_t>(1,this->aencconf.cpr/10));

	if(this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC){
		speed*=10; // dc motors turn at a random speed. reduce the rotation time a bit by increasing openloop speed
	}

	runOpenLoop(bangInitPower, 0, speed, 100,true);

	uint8_t stage = 0;
	int32_t poles = conf.motconf.pole_pairs;
	int32_t initialDirPos = 0;
	while(stage != 3){
		Delay(2);
		if(getPos() > maxpos*poles && stage == 0){
			runOpenLoop(bangInitPower, 0, -speed, 100,true);
			stage = 1;
		}else if(getPos() < minpos*poles && stage == 1){
			// Scale might still be wrong... maxVal-minVal is too high. In theory its 0xffff range and scaler /256. Leave some room to prevent clipping
			aencconf.AENC0_offset = ((maxVal_0 + minVal_0) / 2);
			aencconf.AENC0_scale = 0xF6FF00 / (maxVal_0 - minVal_0);
			if(conf.motconf.enctype == EncoderType_TMC::uvw){
				aencconf.AENC1_offset = ((maxVal_1 + minVal_1) / 2);
				aencconf.AENC1_scale = 0xF6FF00 / (maxVal_1 - minVal_1);
			}

			aencconf.AENC2_offset = ((maxVal_2 + minVal_2) / 2);
			aencconf.AENC2_scale = 0xF6FF00 / (maxVal_2 - minVal_2);
			aencconf.rdir = false;
			setup_AENC(aencconf);
			runOpenLoop(0, 0, 0, 1000,true);
			Delay(250);
			// Zero aenc
			writeReg(0x41, 0);
			initialDirPos = readReg(0x41);
			runOpenLoop(bangInitPower, 0, speed, 100,true);
			stage = 2;
		}else if(getPos() > 0 && stage == 2){
			stage = 3;
			runOpenLoop(0, 0, 0, 1000,true);
		}

		writeReg(0x03,2);
		uint32_t aencUX = readReg(0x02)>>16;
		writeReg(0x03,3);
		uint32_t aencWY_VN = readReg(0x02) ;
		uint32_t aencWY = aencWY_VN >> 16;
		uint32_t aencVN = aencWY_VN & 0xffff;

		minVal_0 = std::min(minVal_0,aencUX);
		minVal_1 = std::min(minVal_1,aencVN);
		minVal_2 = std::min(minVal_2,aencWY);

		maxVal_0 = std::max(maxVal_0,aencUX);
		maxVal_1 = std::max(maxVal_1,aencVN);
		maxVal_2 = std::max(maxVal_2,aencWY);
	}
	// Scale is not actually important. but offset must be perfect
	aencconf.AENC0_offset = ((maxVal_0 + minVal_0) / 2);
	aencconf.AENC0_scale = 0xF6FF00 / (maxVal_0 - minVal_0);
	if(conf.motconf.enctype == EncoderType_TMC::uvw){
		aencconf.AENC1_offset = ((maxVal_1 + minVal_1) / 2);
		aencconf.AENC1_scale = 0xF6FF00 / (maxVal_1 - minVal_1);
	}
	aencconf.AENC2_offset = ((maxVal_2 + minVal_2) / 2);
	aencconf.AENC2_scale = 0xF6FF00 / (maxVal_2 - minVal_2);
	int32_t newDirPos = readReg(0x41);
	aencconf.rdir =  (initialDirPos - newDirPos) > 0;
	setup_AENC(aencconf);
	// Restore settings
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);
	setPosSel(possel);
	setPos(0);

	blinkClipLed(0, 0);
}

/*
 * Steps the motor a few times to check if the encoder follows correctly
 */
bool TMC4671::checkEncoder(){
	if(this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC){ // If not stepper or bldc return
		return true;
	}
	blinkClipLed(150, 0);
	uint8_t phiEreg = 0;
	if(conf.motconf.enctype == EncoderType_TMC::abn){
		phiEreg = 0x2A;
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		phiEreg = 0x46;
	}

	const uint16_t maxcount = 50;
	const int16_t startAngle = 0;
	const int16_t targetAngle = 0x3FFF;

	bool result = true;
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setFluxTorque(0, 0);
	setPhiEtype(PhiE::ext);

	setPhiE_ext(startAngle);
	// Ramp up flux
	for(int16_t flux = 0; flux <= bangInitPower; flux+=20){
		setFluxTorque(flux, 0);
		Delay(2);
	}

	//Forward
	int16_t phiE_enc = 0;
	uint16_t failcount = 0;
	int16_t revCount = 0;
	for(int16_t angle = startAngle;angle<targetAngle;angle+=0x00ff){
		uint16_t c = 0;
		setPhiE_ext(angle);
		Delay(10);
		phiE_enc = (int16_t)(readReg(phiEreg)>>16);
		int16_t err = abs(phiE_enc - angle);
		int16_t nErr = abs(phiE_enc + angle);
		// Wait more
		while(err > 2500 && nErr > 2500 && c++ < 50){
			phiE_enc = (int16_t)(readReg(phiEreg)>>16);
			err = abs(phiE_enc - angle);
			nErr = abs(angle - phiE_enc);
			Delay(10);
		}
		if(err > nErr){
			revCount++;
		}
		if(c >= maxcount){
			failcount++;
			if(failcount > 10){
				result = false;
				break;
			}
		}
	}

	// Backward
	if(result){ // Only if not already failed
		for(int16_t angle = targetAngle;angle>startAngle;angle -= 0x00ff){
			uint16_t c = 0;
			setPhiE_ext(angle);
			Delay(10);
			phiE_enc = (int16_t)(readReg(phiEreg)>>16);
			int16_t err = abs(phiE_enc - angle);
			int16_t nErr = abs(phiE_enc + angle);
			// Wait more
			while(err > 2500 && nErr > 2500 && c++ < 50){
				phiE_enc = (int16_t)(readReg(phiEreg)>>16);
				err = abs(phiE_enc - angle);
				nErr = abs(angle - phiE_enc);
				Delay(10);
			}
			if(err > nErr){
				revCount++;
			}
			if(c >= maxcount){
				failcount++;
				if(failcount > 10){
					result = false;
					break;
				}
			}
		}
	}
	if(revCount > maxcount){ // Encoder seems reversed
		// reverse encoder
		if(this->conf.motconf.enctype == EncoderType_TMC::abn){
			this->abnconf.rdir = !this->abnconf.rdir;
			setup_ABN_Enc(abnconf);
		}else if(this->conf.motconf.enctype == EncoderType_TMC::sincos || this->conf.motconf.enctype == EncoderType_TMC::uvw){
			this->aencconf.rdir = !this->aencconf.rdir;
			setup_AENC(aencconf);
		}
	}

	setFluxTorque(0, 0);
	setPhiE_ext(0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);

	if(result){
		encstate = ENC_InitState::OK;
	}
	blinkClipLed(0, 0);
	return result;
}

void TMC4671::setup_ABN_Enc(TMC4671ABNConf encconf){
	this->abnconf = encconf;

	uint32_t abnmode =
			(encconf.apol |
			(encconf.bpol << 1) |
			(encconf.npol << 2) |
			(encconf.ab_as_n << 3) |
			(encconf.latch_on_N << 8) |
			(encconf.rdir << 12));

	writeReg(0x25, abnmode);
	int32_t pos = getPos();
	writeReg(0x26, encconf.cpr);
	writeReg(0x29, ((uint16_t)encconf.phiEoffset << 16) | (uint16_t)encconf.phiMoffset);
	setPos(pos);
	//writeReg(0x27,0); //Zero encoder
	//conf.motconf.phiEsource = PhiE::abn;


}
void TMC4671::setup_AENC(TMC4671AENCConf encconf){

	// offsets
	writeReg(0x0D,encconf.AENC0_offset | ((uint16_t)encconf.AENC0_scale << 16));
	writeReg(0x0E,encconf.AENC1_offset | ((uint16_t)encconf.AENC1_scale << 16));
	writeReg(0x0F,encconf.AENC2_offset | ((uint16_t)encconf.AENC2_scale << 16));

	writeReg(0x40,encconf.cpr);
	writeReg(0x3e,(uint16_t)encconf.phiAoffset);
	writeReg(0x45,(uint16_t)encconf.phiEoffset | ((uint16_t)encconf.phiMoffset << 16));
	writeReg(0x3c,(uint16_t)encconf.nThreshold | ((uint16_t)encconf.nMask << 16));

	uint32_t mode = encconf.uvwmode & 0x1;
	mode |= (encconf.rdir & 0x1) << 12;
	writeReg(0x3b, mode);

}
void TMC4671::setup_HALL(TMC4671HALLConf hallconf){
	this->hallconf = hallconf;

	uint32_t hallmode =
			hallconf.polarity |
			hallconf.filter << 4 |
			hallconf.interpolation << 8 |
			hallconf.direction << 12 |
			(hallconf.blank & 0xfff) << 16;
	writeReg(0x33, hallmode);
	// Positions
	uint32_t posA = (uint16_t)hallconf.pos0 | (uint16_t)hallconf.pos60 << 16;
	writeReg(0x34, posA);
	uint32_t posB = (uint16_t)hallconf.pos120 | (uint16_t)hallconf.pos180 << 16;
	writeReg(0x35, posB);
	uint32_t posC = (uint16_t)hallconf.pos240 | (uint16_t)hallconf.pos300 << 16;
	writeReg(0x36, posC);

	uint32_t phiOffsets = (uint16_t)hallconf.phiMoffset | (uint16_t)hallconf.phiEoffset << 16;
	writeReg(0x37, phiOffsets);
	writeReg(0x38, hallconf.dPhiMax);

	//conf.motconf.phiEsource = PhiE::hall;
}


/*
 * Calibrates the ADC by disabling the power stage and sampling a mean value. Takes time!
 */
bool TMC4671::calibrateAdcOffset(uint16_t time){

	uint16_t measuretime_idle = time;
	uint32_t measurements_idle = 0;
	uint64_t totalA=0;
	uint64_t totalB=0;

	writeReg(0x03, 0); // Read raw adc
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setMotionMode(MotionMode::stop,true);

	uint16_t lastrawA=conf.adc_I0_offset, lastrawB=conf.adc_I1_offset;

	//pulseClipLed(); // Turn on led
	// Disable drivers and measure many samples of zero current
	//enablePin.reset();
	uint32_t tick = HAL_GetTick();
	while(HAL_GetTick() - tick < measuretime_idle){ // Measure idle
		uint32_t adcraw = readReg(0x02);
		uint16_t rawA = adcraw & 0xffff;
		uint16_t rawB = (adcraw >> 16) & 0xffff;
		// Signflip filter for SPI bug
		if(abs(lastrawA-rawA) < 10000 && abs(lastrawB-rawB) < 10000){
			totalA += rawA;
			totalB += rawB;
			measurements_idle++;
			lastrawA = rawA;
			lastrawB = rawB;
		}
	}
	//enablePin.set();
	uint32_t offsetAidle = totalA / (measurements_idle);
	uint32_t offsetBidle = totalB / (measurements_idle);

	// Check if offsets are in a valid range
	if(totalA < 100 || totalB < 100 || (abs((int32_t)offsetAidle - 0x7fff) > 5000 || abs((int32_t)offsetBidle - 0x7fff) > 5000)){
		ErrorHandler::addError(Error(ErrorCode::adcCalibrationError,ErrorType::critical,"TMC Adc/Shunt offset calibration failed."));
		blinkErrLed(100, 0); // Blink forever
		this->changeState(TMC_ControlState::HardError);
		return false; // An adc or shunt amp is likely broken. do not proceed.
	}
	conf.adc_I0_offset = offsetAidle;
	conf.adc_I1_offset = offsetBidle;
	setAdcOffset(conf.adc_I0_offset, conf.adc_I1_offset);
	// ADC Offsets should now be close to perfect

	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);
	return true;
}


void TMC4671::ABN_init(){

	switch(encstate){
		case ENC_InitState::uninitialized:
			setPosSel(PosSelection::PhiM_abn); // Mechanical Angle
			writeReg(0x26, abnconf.cpr); // we need cpr to be set first

			if(this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC){
				setPhiEtype(PhiE::abn);
				encstate = ENC_InitState::OK; // Skip for DC motors
				if(manualEncAlign){
					CommandHandler::sendSerial("encalign","DC motors don't support alignment",this->getInfo().unique);
				}
			}else{
				encstate = ENC_InitState::estimating;
			}

		break;

		case ENC_InitState::estimating:
		{
			if(!hasPower())
				break;
			int32_t pos = getPos();
			setPos(0);
			bool olddir = abnconf.rdir;
			estimateABNparams();

			if(olddir != this->abnconf.rdir){ // Last measurement should be reversed
				pos = -getPos()-pos;
			}

			setPos(pos);
			setup_ABN_Enc(this->abnconf);
			encstate = ENC_InitState::aligning;
		}
		break;

		case ENC_InitState::aligning:
			if(hasPower()){
				bangInitEnc(bangInitPower);
				encstate = ENC_InitState::checking;
			}
		break;

		case ENC_InitState::checking:
			if(checkEncoder()){
				encstate = ENC_InitState::OK;
				setPhiEtype(PhiE::abn);
				enc_retry = 0;
				if(manualEncAlign){
					manualEncAlign = false;
					CommandHandler::sendSerial("encalign","Aligned successfully",this->getInfo().unique);
				}

			}else{
				blinkErrLed(100, 10);
				if(enc_retry++ > enc_retry_max){
					blinkErrLed(50, 50);
					changeState(TMC_ControlState::HardError);
					Error err = Error(ErrorCode::encoderAlignmentFailed,ErrorType::critical,"Encoder alignment failed multiple times");
					ErrorHandler::addError(err);
					if(manualEncAlign){
						manualEncAlign = false;
						CommandHandler::sendSerial("encalign","Error aligning.\nPlease check settings and reset.",this->getInfo().unique);
					}
				}
				encstate = ENC_InitState::uninitialized; // Retry
			}
		break;
		case ENC_InitState::OK:
			changeState(TMC_ControlState::EncoderFinished);
			break;
	}
}

void TMC4671::AENC_init(){
	if(this->conf.motconf.motor_type == MotorType::NONE){
		encstate = ENC_InitState::OK;
	}
	switch(encstate){
		case ENC_InitState::uninitialized:
			setPosSel(PosSelection::PhiM_aenc);
			setPos(0);
			setup_AENC(this->aencconf);
			encstate = ENC_InitState::estimating;
		break;

		case ENC_InitState::estimating:
			if(!hasPower())
				break;
			calibrateAenc();
			encstate = ENC_InitState::aligning;
		break;

		case ENC_InitState::aligning:
			if(!hasPower())
				break;
			bangInitEnc(bangInitPower);
			encstate = ENC_InitState::checking;
		break;

		case ENC_InitState::checking:
			if(checkEncoder()){
				encstate =ENC_InitState::OK;
				setPhiEtype(PhiE::aenc);
				enc_retry = 0;
				if(manualEncAlign){
					manualEncAlign = false;
					CommandHandler::sendSerial("encalign","Aligned successfully",this->getInfo().unique);
				}


			}else{
				blinkErrLed(100, 10);
				if(enc_retry++ > enc_retry_max){
					blinkErrLed(50, 50);
					changeState(TMC_ControlState::HardError);
					Error err = Error(ErrorCode::encoderAlignmentFailed,ErrorType::critical,"Encoder alignment failed multiple times");
					ErrorHandler::addError(err);
					if(manualEncAlign){
						manualEncAlign = false;
						CommandHandler::sendSerial("encalign","Error aligning.\nPlease check settings and reset.",this->getInfo().unique);
					}
				}
				encstate = ENC_InitState::uninitialized; // Retry
			}
		break;
		case ENC_InitState::OK:
			changeState(TMC_ControlState::EncoderFinished);
			break;
	}
}

/*
 * Changes the encoder type and calls init methods for the encoder types.
 * Setup the specific parameters (abnconf, aencconf...) first.
 */
void TMC4671::setEncoderType(EncoderType_TMC type){
	this->conf.motconf.enctype = type;
	this->statusMask.flags.AENC_N = 0;
	this->statusMask.flags.ENC_N = 0;
	setStatusMask(statusMask);
	if(type == EncoderType_TMC::abn){

		// Not initialized if cpr not set
		if(this->abnconf.cpr == 0){
			return;
		}
		this->statusMask.flags.ENC_N = 1;
		changeState(TMC_ControlState::ABN_init);
		encstate = ENC_InitState::uninitialized;

	// SinCos encoder
	}else if(type == EncoderType_TMC::sincos){
		changeState(TMC_ControlState::AENC_init);
		encstate = ENC_InitState::uninitialized;
		this->aencconf.uvwmode = false; // sincos mode
		this->statusMask.flags.AENC_N = 1;
	// Analog UVW encoder
	}else if(type == EncoderType_TMC::uvw){
		changeState(TMC_ControlState::AENC_init);
		encstate = ENC_InitState::uninitialized;
		this->aencconf.uvwmode = true; // uvw mode

	}else if(type == EncoderType_TMC::hall){ // Hall sensor. Just trust it
		changeState(TMC_ControlState::Running);
		setPosSel(PosSelection::PhiM_hal);
		encstate = ENC_InitState::OK;
		setPhiEtype(PhiE::hall);
	}

}

uint32_t TMC4671::getEncCpr(){
	EncoderType_TMC type = conf.motconf.enctype;
	if(type == EncoderType_TMC::abn){
		return abnconf.cpr;
	}else if(type == EncoderType_TMC::sincos || type == EncoderType_TMC::uvw){
		return aencconf.cpr;
	}
	else{
		return getCpr();
	}
}

void TMC4671::setPhiEtype(PhiE type){
	conf.motconf.phiEsource = type;
	writeReg(0x52, (uint8_t)type & 0xff);
}
PhiE TMC4671::getPhiEtype(){
	return PhiE(readReg(0x52) & 0x7);
}

void TMC4671::setMotionMode(MotionMode mode, bool force){
	if(!force){
		nextMotionMode = mode;
		return;
	}
	if(mode != curMotionMode){
		lastMotionMode = curMotionMode;
	}
	curMotionMode = mode;
	updateReg(0x63, (uint8_t)mode, 0xff, 0);
}
MotionMode TMC4671::getMotionMode(){
	curMotionMode = MotionMode(readReg(0x63) & 0xff);
	return curMotionMode;
}

void TMC4671::setOpenLoopSpeedAccel(int32_t speed,uint32_t accel){
	writeReg(0x21, speed);
	writeReg(0x20, accel);
}


void TMC4671::runOpenLoop(uint16_t ud,uint16_t uq,int32_t speed,int32_t accel,bool torqueMode){
	if(this->conf.motconf.motor_type == MotorType::DC){
		uq = ud+uq; // dc motor has no flux. add to torque
	}
	if(torqueMode){
		if(this->conf.motconf.motor_type == MotorType::DC){
			uq = ud+uq; // dc motor has no flux. add to torque
		}
		setFluxTorque(ud, uq);
	}else{
		setMotionMode(MotionMode::uqudext,true);
		setUdUq(ud,uq);
	}
	setPhiEtype(PhiE::openloop);

	setOpenLoopSpeedAccel(speed, accel);
}

void TMC4671::setUdUq(int16_t ud,int16_t uq){
	writeReg(0x24, ud | (uq << 16));
}

void TMC4671::stopMotor(){
	// Stop driver if running

	//enablePin.reset();
	active = false;
	if(state == TMC_ControlState::Running){
		setMotionMode(MotionMode::stop,true);
		//setPwm(0); // disable foc
		changeState(TMC_ControlState::Shutdown);
	}
}
void TMC4671::startMotor(){
	active = true;
	if(!initialized || emergency){
		initialize();
		emergency = false;
	}

	if(state == TMC_ControlState::Shutdown && initialized && encstate == ENC_InitState::OK){
		changeState(TMC_ControlState::Running);
	}
	// Start driver if powered
	if(hasPower()){
		enablePin.set();
		setPwm(7); // enable foc
		setMotionMode(nextMotionMode,true);

	}else{
		changeState(TMC_ControlState::Init_wait);
	}

}

void TMC4671::emergencyStop(){
	setPwm(1); // Short low side for instant stop
	emergency = true;
	enablePin.reset();
	active = false;
	changeState(TMC_ControlState::HardError);
}

/*
 * Sets a torque in positive or negative direction
 * For ADC linearity reasons under 25000 is recommended
 */
void TMC4671::turn(int16_t power){
	if(!(this->motorReady() && active))
		return;
	int32_t flux = 0;

	// Flux offset for field weakening
	//if(this->conf.motconf.motor_type == MotorType::STEPPER){
	flux = idleFlux-clip<int32_t,int16_t>(abs(power),0,maxOffsetFlux);
	//}
	if(feedforward){
		setFluxTorque(flux, power/2);
		setFluxTorqueFF(0, power/2);
	}else{
		setFluxTorque(flux, power);
	}
}

void TMC4671::setPosSel(PosSelection psel){
	writeReg(0x51, (uint8_t)psel);
	this->conf.motconf.pos_sel = psel;
}

int32_t TMC4671::getPos(){
	//int64_t cpr = conf.motconf.pole_pairs << 16;
	/*
	int32_t mpos = (int32_t)readReg(0x6B) / ((int32_t)conf.motconf.pole_pairs);
	int32_t pos = ((int32_t)abnconf.cpr * mpos) >> 16;*/
	int32_t pos = (int32_t)readReg(0x6B);
	if(this->conf.motconf.phiEsource == PhiE::abn){
		int64_t tmpos = ( (int64_t)pos * (int64_t)abnconf.cpr);
		pos = tmpos / 0xffff;
	}

	return pos;
}

Encoder* TMC4671::getEncoder(){
	return static_cast<Encoder*>(this);
}

bool TMC4671::hasIntegratedEncoder(){
	return true;
}

void TMC4671::setPos(int32_t pos){
	// Cpr = poles * 0xffff
	/*
	int32_t cpr = (conf.motconf.pole_pairs << 16) / abnconf.cpr;
	int32_t mpos = (cpr * pos);*/
	if(this->conf.motconf.phiEsource == PhiE::abn){
		pos = ((int64_t)0xffff / (int64_t)abnconf.cpr) * (int64_t)pos;

	}
	writeReg(0x6B, pos);
}


uint32_t TMC4671::getCpr(){
	if(this->conf.motconf.phiEsource == PhiE::abn){
		return abnconf.cpr;
	}else{
		return 0xffff;
	}

}
void TMC4671::setCpr(uint32_t cpr){
	if(cpr == 0)
		cpr = 1;

	this->abnconf.cpr = cpr;
	this->aencconf.cpr = cpr;
	writeReg(0x26, abnconf.cpr); //ABN
	writeReg(0x40, aencconf.cpr); //AENC

}

uint32_t TMC4671::encToPos(uint32_t enc){
	return enc*(0xffff / abnconf.cpr)*(conf.motconf.pole_pairs);
}
uint32_t TMC4671::posToEnc(uint32_t pos){
	return pos/((0xffff / abnconf.cpr)*(conf.motconf.pole_pairs)) % abnconf.cpr;
}

EncoderType TMC4671::getType(){
	return EncoderType::incremental;
}



void TMC4671::setAdcOffset(uint32_t adc_I0_offset,uint32_t adc_I1_offset){
	conf.adc_I0_offset = adc_I0_offset;
	conf.adc_I1_offset = adc_I1_offset;

	updateReg(0x09, adc_I0_offset, 0xffff, 0);
	updateReg(0x08, adc_I1_offset, 0xffff, 0);
}

void TMC4671::setAdcScale(uint32_t adc_I0_scale,uint32_t adc_I1_scale){
	conf.adc_I0_scale = adc_I0_scale;
	conf.adc_I1_scale = adc_I1_scale;

	updateReg(0x09, adc_I0_scale, 0xffff, 16);
	updateReg(0x08, adc_I1_scale, 0xffff, 16);
}

void TMC4671::setupFeedForwardTorque(int32_t gain, int32_t constant){
	writeReg(0x4E, 42);
	writeReg(0x4D, gain);
	writeReg(0x4E, 43);
	writeReg(0x4D, constant);
}
void TMC4671::setupFeedForwardVelocity(int32_t gain, int32_t constant){
	writeReg(0x4E, 40);
	writeReg(0x4D, gain);
	writeReg(0x4E, 41);
	writeReg(0x4D, constant);
}

void TMC4671::setFFMode(FFMode mode){
	updateReg(0x63, (uint8_t)mode, 0xff, 16);
	if(mode!=FFMode::none){
		feedforward = true;
		setSequentialPI(true);
	}else{
		feedforward = false;
	}
}

void TMC4671::setSequentialPI(bool sequential){
	curPids.sequentialPI = sequential;
	updateReg(0x63, sequential ? 1 : 0, 0x1, 31);
}

void TMC4671::setMotorType(MotorType motor,uint16_t poles){
	if(motor == MotorType::DC){
		poles = 1;
	}
	conf.motconf.motor_type = motor;
	conf.motconf.pole_pairs = poles;
	uint32_t mtype = poles | ( ((uint8_t)motor&0xff) << 16);
	if(motor != MotorType::STEPPER){
		maxOffsetFlux = 0; // Offsetflux only helpful for steppers. Has no effect otherwise
	}
	writeReg(0x1B, mtype);
	if(motor == MotorType::BLDC && !oldTMCdetected){
		setSvPwm(useSvPwm); // Higher speed for BLDC motors. Not available in engineering samples
	}
}

void TMC4671::setTorque(int16_t torque){
	if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque,true);
	}
	updateReg(0x64,torque,0xffff,16);
}
int16_t TMC4671::getTorque(){
	return readReg(0x64) >> 16;
}

void TMC4671::setFlux(int16_t flux){
	if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque,true);
	}
	updateReg(0x64,flux,0xffff,0);
}
int16_t TMC4671::getFlux(){
	return readReg(0x64) && 0xffff;
}
void TMC4671::setFluxTorque(int16_t flux, int16_t torque){
	if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque,true);
	}
	writeReg(0x64, (flux & 0xffff) | (torque << 16));
}

void TMC4671::setFluxTorqueFF(int16_t flux, int16_t torque){
	if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque,true);
	}
	writeReg(0x65, (flux & 0xffff) | (torque << 16));
}


void TMC4671::setPids(TMC4671PIDConf pids){
	curPids = pids;
	writeReg(0x54, pids.fluxI | (pids.fluxP << 16));
	writeReg(0x56, pids.torqueI | (pids.torqueP << 16));
	writeReg(0x58, pids.velocityI | (pids.velocityP << 16));
	writeReg(0x5A, pids.positionI | (pids.positionP << 16));
	setSequentialPI(pids.sequentialPI);
}

TMC4671PIDConf TMC4671::getPids(){
	uint32_t f = readReg(0x54);
	uint32_t t = readReg(0x56);
	uint32_t v = readReg(0x58);
	uint32_t p = readReg(0x5A);
	// Update pid storage
	curPids = {(uint16_t)(f&0xffff),(uint16_t)(f>>16),(uint16_t)(t&0xffff),(uint16_t)(t>>16),(uint16_t)(v&0xffff),(uint16_t)(v>>16),(uint16_t)(p&0xffff),(uint16_t)(p>>16)};
	return curPids;
}

/*
 * Limits the PWM value
 */
void TMC4671::setUqUdLimit(uint16_t limit){
	this->curLimits.pid_uq_ud = limit;
	writeReg(0x5D, limit);
}

void TMC4671::setTorqueLimit(uint16_t limit){
	this->curLimits.pid_torque_flux = limit;
	writeReg(0x5E, limit);
}

void TMC4671::setPidPrecision(TMC4671PidPrecision setting){

	this->pidPrecision = setting;
	uint16_t dat = setting.current_I;
	dat |= setting.current_P << 1;
	dat |= setting.velocity_I << 2;
	dat |= setting.velocity_P << 3;
	dat |= setting.position_I << 4;
	dat |= setting.position_P << 5;
	writeReg(0x4E, 62); // set config register address
	writeReg(0x4D, dat);
}

void TMC4671::setLimits(TMC4671Limits limits){
	this->curLimits = limits;
	writeReg(0x5C, limits.pid_torque_flux_ddt);
	writeReg(0x5D, limits.pid_uq_ud);
	writeReg(0x5E, limits.pid_torque_flux);
	writeReg(0x5F, limits.pid_acc_lim);
	writeReg(0x60, limits.pid_vel_lim);
	writeReg(0x61, limits.pid_pos_low);
	writeReg(0x62, limits.pid_pos_high);
}

TMC4671Limits TMC4671::getLimits(){
	curLimits.pid_acc_lim = readReg(0x5F);
	curLimits.pid_torque_flux = readReg(0x5E);
	curLimits.pid_torque_flux_ddt = readReg(0x5C);
	curLimits.pid_uq_ud= readReg(0x5D);
	curLimits.pid_vel_lim = readReg(0x60);
	curLimits.pid_pos_low = readReg(0x61);
	curLimits.pid_pos_high = readReg(0x62);
	return curLimits;
}

void TMC4671::setBiquadFlux(TMC4671Biquad bq){
	writeReg(0x4E, 25);
	writeReg(0x4D, bq.a1);
	writeReg(0x4E, 26);
	writeReg(0x4D, bq.a2);
	writeReg(0x4E, 28);
	writeReg(0x4D, bq.b0);
	writeReg(0x4E, 29);
	writeReg(0x4D, bq.b1);
	writeReg(0x4E, 30);
	writeReg(0x4D, bq.b2);
	writeReg(0x4E, 31);
	writeReg(0x4D, bq.enable & 0x1);
}
void TMC4671::setBiquadPos(TMC4671Biquad bq){
	writeReg(0x4E, 1);
	writeReg(0x4D, bq.a1);
	writeReg(0x4E, 2);
	writeReg(0x4D, bq.a2);
	writeReg(0x4E, 4);
	writeReg(0x4D, bq.b0);
	writeReg(0x4E, 5);
	writeReg(0x4D, bq.b1);
	writeReg(0x4E, 6);
	writeReg(0x4D, bq.b2);
	writeReg(0x4E, 7);
	writeReg(0x4D, bq.enable & 0x1);
}
void TMC4671::setBiquadVel(TMC4671Biquad bq){
	writeReg(0x4E, 9);
	writeReg(0x4D, bq.a1);
	writeReg(0x4E, 10);
	writeReg(0x4D, bq.a2);
	writeReg(0x4E, 12);
	writeReg(0x4D, bq.b0);
	writeReg(0x4E, 13);
	writeReg(0x4D, bq.b1);
	writeReg(0x4E, 14);
	writeReg(0x4D, bq.b2);
	writeReg(0x4E, 15);
	writeReg(0x4D, bq.enable & 0x1);
}
void TMC4671::setBiquadTorque(TMC4671Biquad bq){
	writeReg(0x4E, 17);
	writeReg(0x4D, bq.a1);
	writeReg(0x4E, 18);
	writeReg(0x4D, bq.a2);
	writeReg(0x4E, 20);
	writeReg(0x4D, bq.b0);
	writeReg(0x4E, 21);
	writeReg(0x4D, bq.b1);
	writeReg(0x4E, 22);
	writeReg(0x4D, bq.b2);
	writeReg(0x4E, 23);
	writeReg(0x4D, bq.enable & 0x1);
}

/*
 *  Sets the raw brake resistor limits.
 *  Centered at 0x7fff
 *  Set both 0 to deactivate
 */
void TMC4671::setBrakeLimits(uint16_t low,uint16_t high){
	uint32_t val = low | (high << 16);
	writeReg(0x75,val);
}

/*
 * Moves the rotor and estimates polarity and direction of the encoder
 * Polarity is found by measuring the n pulse.
 * If polarity was found to be reversed during the test direction will be reversed again to account for that
 */
void TMC4671::estimateABNparams(){
	blinkClipLed(100, 0);
	int32_t pos = getPos();
	setPos(0);
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	updateReg(0x25, 0,0x1000,12); // Set dir normal
	setPhiE_ext(0);
	setPhiEtype(PhiE::ext);
	setFluxTorque(0, 0);
	setMotionMode(MotionMode::torque,true);
	for(int16_t flux = 0; flux <= bangInitPower; flux+=10){
		setFluxTorque(flux, 0);
		Delay(5);
	}


	int16_t phiE_abn = readReg(0x2A)>>16;
	int16_t phiE_abn_old = 0;
	int16_t rcount=0,c = 0; // Count how often direction was in reverse
	uint16_t highcount = 0; // Count high state of n pulse for polarity estimation

	// Rotate a bit
	for(int16_t p = 0;p<0x0fff;p+=0x2f){
		setPhiE_ext(p);
		Delay(10);
		c++;
		phiE_abn_old = phiE_abn;
		phiE_abn = readReg(0x2A)>>16;
		if(phiE_abn < phiE_abn_old){
			rcount++;
		}
		if((readReg(0x76) & 0x04) >> 2){
			highcount++;
		}
	}
	setPos(pos+getPos());

	setFluxTorque(0, 0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);

	bool npol = highcount > c/2;
	abnconf.rdir = rcount > c/2;
//	if(npol != abnconf.npol) // Invert dir if polarity was reversed TODO correct? likely wrong at the moment
//		abnconf.rdir = !abnconf.rdir;

	abnconf.apol = npol;
	abnconf.bpol = npol;
	abnconf.npol = npol;
	blinkClipLed(0, 0);
}




void TMC4671::setPwm(uint8_t val){
	updateReg(0x1A,val,0xff,0);
}

void TMC4671::setPwm(uint8_t val,uint16_t maxcnt,uint8_t bbmL,uint8_t bbmH){
	writeReg(0x18, maxcnt);
	updateReg(0x1A,val,0xff,0);
	uint32_t bbmr = bbmL | (bbmH << 8);
	writeReg(0x19, bbmr);
	writeReg(0x17,0); //Polarity
}

void TMC4671::setSvPwm(bool enable){
	updateReg(0x1A,enable,0x01,8);
}


void TMC4671::initAdc(uint16_t mdecA, uint16_t mdecB,uint32_t mclkA,uint32_t mclkB){
	uint32_t dat = mdecA | (mdecB << 16);
	writeReg(0x07, dat);

	writeReg(0x05, mclkA);
	writeReg(0x06, mclkB);
	// Enable/Disable adcs
	updateReg(0x04, mclkA == 0 ? 0 : 1, 0x1, 4);
	updateReg(0x04, mclkB == 0 ? 0 : 1, 0x1, 20);

	writeReg(0x0A,0x18000100); // ADC Selection
}

/*
 * Returns measured flux and torque as a pair
 * Flux is first, torque second item
 */
std::pair<int32_t,int32_t> TMC4671::getActualCurrent(){
	uint32_t tfluxa = readReg(0x69);
	int16_t af = (tfluxa & 0xffff);
	int16_t at = (tfluxa >> 16);
	return std::pair<int16_t,int16_t>(af,at);
}

//__attribute__((optimize("-Ofast")))
uint32_t TMC4671::readReg(uint8_t reg){
	spiPort.takeSemaphore();
	uint8_t req[5] = {(uint8_t)(0x7F & reg),0,0,0,0};
	uint8_t tbuf[5];
	// 500ns delay after sending first byte
	spiPort.transmitReceive(req, tbuf, 5,this, SPITIMEOUT);
	uint32_t ret;
	memcpy(&ret,tbuf+1,4);
	ret = __REV(ret);

	return ret;
}

//__attribute__((optimize("-Ofast")))
void TMC4671::writeReg(uint8_t reg,uint32_t dat){

	// wait until ready
	spiPort.takeSemaphore();
	spi_buf[0] = (uint8_t)(0x80 | reg);
	dat =__REV(dat);
	memcpy(spi_buf+1,&dat,4);

	// -----
	//spiPort.transmit_DMA(this->spi_buf, 5, this);
	spiPort.transmit(spi_buf, 5, this, SPITIMEOUT);
}

void TMC4671::updateReg(uint8_t reg,uint32_t dat,uint32_t mask,uint8_t shift){

	uint32_t t = readReg(reg) & ~(mask << shift);
	t |= ((dat & mask) << shift);
	writeReg(reg, t);
}

void TMC4671::beginSpiTransfer(SPIPort* port){
	assertChipSelect();
}
void TMC4671::endSpiTransfer(SPIPort* port){
	clearChipSelect();
	port->giveSemaphore();
}

/**
 * Reads status flags
 * @param maskedOnly Masks flags by previously set flag mask that would trigger an interrupt. False to read all flags
 */
StatusFlags TMC4671::readFlags(bool maskedOnly){
	uint32_t flags = readReg(0x7C);
	if(maskedOnly){
		flags = flags & this->statusMask.asInt;
	}
	this->statusFlags.asInt = flags; // Only set flags that are marked to trigger a notification
	return statusFlags;
}

void TMC4671::setStatusMask(StatusFlags mask){
	writeReg(0x7D, mask.asInt);
}

void TMC4671::setStatusMask(uint32_t mask){
	writeReg(0x7D, mask);
}

void TMC4671::setStatusFlags(uint32_t flags){
	writeReg(0x7C, flags);
}

void TMC4671::setStatusFlags(StatusFlags flags){
	writeReg(0x7C, flags.asInt);
}

/**
 * Reads and resets all status flags and executes depending on status flags
 */
void TMC4671::statusCheck(){
	statusFlags = readFlags(); // Update current flags
	setStatusFlags(0); // Reset flags

	// encoder index flag was set since last check. Check if the flag matching the current encoder is set
	if( (statusFlags.flags.ENC_N && this->conf.motconf.enctype == EncoderType_TMC::abn) || (statusFlags.flags.AENC_N && this->conf.motconf.enctype == EncoderType_TMC::sincos) ){
		encoderIndexHit();
	}

	if(statusFlags.flags.not_PLL_locked){
		// Critical error. PLL not locked
		ErrorHandler::addError(Error(ErrorCode::tmcPLLunlocked, ErrorType::critical, "TMC PLL not locked"));
	}
}

void TMC4671::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == FLAG_Pin){ // Flag pin went high
		statusCheck();
	}
}

void TMC4671::encoderIndexHit(){

}

TMC4671MotConf TMC4671::decodeMotFromInt(uint16_t val){
	// 0-2: MotType 3-5: Encoder source 6-15: Poles
	TMC4671MotConf mot;
	mot.motor_type = MotorType(val & 0x7);
	mot.enctype = EncoderType_TMC( (val >> 3) & 0x7);
	mot.pole_pairs = val >> 6;
	return mot;
}
uint16_t TMC4671::encodeMotToInt(TMC4671MotConf mconf){
	uint16_t val = (uint8_t)mconf.motor_type & 0x7;
	val |= ((uint8_t)mconf.enctype & 0x7) << 3;
	val |= (mconf.pole_pairs & 0x3FF) << 6;
	return val;
}

uint16_t TMC4671::encodeEncHallMisc(){
	uint16_t val = 0;
	val |= (this->abnconf.npol) & 0x01;
	val |= (this->abnconf.rdir & 0x01)  << 1; // Direction
	val |= (this->abnconf.ab_as_n & 0x01) << 2;
	val |= (this->pidPrecision.current_I) << 3;
	val |= (this->pidPrecision.current_P) << 4;

	val |= (this->hallconf.direction & 0x01) << 8;
	val |= (this->hallconf.interpolation & 0x01) << 9;

	val |= (this->curPids.sequentialPI & 0x01) << 10;

	//val |= (this->getSpiAddr() << 14);
	return val;
}

void TMC4671::restoreEncHallMisc(uint16_t val){

	this->abnconf.apol = (val) & 0x01;
	this->abnconf.bpol = this->abnconf.apol;
	this->abnconf.npol = this->abnconf.apol;
	this->abnconf.rdir = (val>>1) & 0x01; // Direction
	this->abnconf.ab_as_n = (val>>2) & 0x01;
	this->pidPrecision.current_I = (val>>3) & 0x01;
	this->pidPrecision.velocity_I = this->pidPrecision.current_I;
	this->pidPrecision.position_I = this->pidPrecision.current_I;
	this->pidPrecision.current_P = (val>>4) & 0x01;
	this->pidPrecision.velocity_P = this->pidPrecision.current_P;
	this->pidPrecision.velocity_P = this->pidPrecision.current_P;

	this->hallconf.direction = (val>>8) & 0x01;
	this->hallconf.interpolation = (val>>9) & 0x01;

	this->curPids.sequentialPI = (val>>10) & 0x01;

//	uint8_t chip_sel = (val>>14) & 0x03;
//	// try to set the CS to the stored value
//	if(!setSpiAddr(chip_sel)){
//		// else try the default value, else panic!
//		assert(setSpiAddr(0));
//	}
}


ParseStatus TMC4671::command(ParsedCommand* cmd,std::string* reply){
	char axis = cmd->prefix & 0xDF;
	if (axis != 0 && axis != this->axis){
		return ParseStatus::NOT_FOUND;
	}

	ParseStatus flag = ParseStatus::OK;
	if(cmd->cmd == "mtype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.motconf.motor_type);
		}else if(cmd->type == CMDtype::set && (uint8_t)cmd->type < (uint8_t)MotorType::ERR){
			this->setMotorType((MotorType)cmd->val, this->conf.motconf.pole_pairs);
		}else{
			*reply+="NONE=0,DC=1,STEPPER=2,BLDC=3";
		}

	}else if(cmd->cmd == "encsrc"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.motconf.enctype);
		}else if(cmd->type == CMDtype::set){
			this->setEncoderType((EncoderType_TMC)cmd->val);
		}else{
			*reply+="NONE=0,ABN=1,SinCos=2,Analog UVW=3,Hall=4";
		}

	}else if(cmd->cmd == "encalign"){
		if(cmd->type == CMDtype::get){
			encstate = ENC_InitState::uninitialized;
			this->setEncoderType(this->conf.motconf.enctype);
			manualEncAlign = true;
			flag = ParseStatus::NO_REPLY;
		}
	}else if(cmd->cmd == "poles"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->conf.motconf.pole_pairs);
		}else if(cmd->type == CMDtype::set){
			this->setMotorType(this->conf.motconf.motor_type,cmd->val);
		}

	}else if(cmd->cmd == "cprtmc"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(getEncCpr());
		}else if(cmd->type == CMDtype::set){
			this->setCpr(cmd->val);
		}

	}else if(cmd->cmd == "acttrq"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(getActualCurrent().second);
		}

	}else if(cmd->cmd == "tmcpwmlim"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->curLimits.pid_uq_ud);
		}else if(cmd->type == CMDtype::set){
			this->setUqUdLimit(cmd->val);
		}

	}else if(cmd->cmd == "torqueP"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->curPids.torqueP);
		}else if(cmd->type == CMDtype::set){
			this->curPids.torqueP = cmd->val;
			setPids(curPids);
		}
	}else if(cmd->cmd == "torqueI"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->curPids.torqueI);
		}else if(cmd->type == CMDtype::set){
			this->curPids.torqueI = cmd->val;
			setPids(curPids);
		}
	}else if(cmd->cmd == "fluxP"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->curPids.fluxP);
		}else if(cmd->type == CMDtype::set){
			this->curPids.fluxP = cmd->val;
			setPids(curPids);
		}
	}else if(cmd->cmd == "fluxI"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->curPids.fluxI);
		}else if(cmd->type == CMDtype::set){
			this->curPids.fluxI = cmd->val;
			setPids(curPids);
		}
	}else if(cmd->cmd == "pidPrec"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->pidPrecision.current_I | (this->pidPrecision.current_P << 1));
		}else if(cmd->type == CMDtype::set){
			this->pidPrecision.current_I = cmd->val & 0x1;
			this->pidPrecision.current_P = (cmd->val >> 1) & 0x1;
			this->setPidPrecision(pidPrecision);
		}else{
			*reply+="bit0=I bit1=P. 0=Q8.8 1= Q4.12";
		}

	}else if(cmd->cmd == "phiesrc"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->getPhiEtype());
		}else if(cmd->type == CMDtype::set){
			this->setPhiEtype((PhiE)cmd->val);
		}else{
			*reply+="ext=1,openloop=2,abn=3,hall=5,aenc=6,aencE=7";
		}

	}else if(cmd->cmd == "fluxoffset"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->maxOffsetFlux);
		}else if(cmd->type == CMDtype::set){
			this->maxOffsetFlux = cmd->val;
		}

	}else if(cmd->cmd == "reg"){
		if(cmd->type == CMDtype::getat){
			*reply+=std::to_string(this->readReg(cmd->val));
		}else if(cmd->type == CMDtype::setat){
			this->writeReg(cmd->adr,cmd->val);
		}

	}else if(cmd->cmd == "seqpi"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->curPids.sequentialPI);
		}else if(cmd->type == CMDtype::set){
			this->setSequentialPI(cmd->val != 0);
		}

	}else if(cmd->cmd == "tmcIscale"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->currentScaler);
		}
	}else if(cmd->cmd == "encdir"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->abnconf.rdir);
		}else if(cmd->type == CMDtype::set){
			this->abnconf.rdir = cmd->val != 0;
			this->setup_ABN_Enc(this->abnconf);
		}

	}else if(cmd->cmd == "tmctemp"){
		if(cmd->type == CMDtype::get){
#ifdef TMCTEMP
			*reply+=std::to_string(this->getTemp());
#else
			*reply+="0";
#endif

		}
	}else{
		flag = ParseStatus::NOT_FOUND;
	}
	return flag;
}

