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

ClassIdentifier TMC4671::info = {
		 .name = "TMC4671" ,
		 .id=1
 };
const ClassIdentifier TMC4671::getInfo(){
	return info;
}

TMC4671::TMC4671(SPI_HandleTypeDef* spi,GPIO_TypeDef* csport,uint16_t cspin,TMC4671MainConfig conf) {
	this->cspin = cspin;
	this->csport = csport;
	this->spi = spi;
	this->conf = conf;
}

TMC4671::TMC4671(){
	this->address = 1;
	this->cspin = SPI1_SS1_Pin;
	this->csport = SPI1_SS1_GPIO_Port;
	this->spi = &HSPIDRV;

}


TMC4671::~TMC4671() {
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_RESET);
}



/*
 * Address can be set to automatically setup spi and
 * load constants from flash
 */
void TMC4671::setAddress(uint8_t addr){
	this->address = addr;

	// Auto setup spi
	if(addr == 1){
		this->cspin = SPI1_SS1_Pin;
		this->csport = SPI1_SS1_GPIO_Port;
		this->spi = &HSPIDRV;
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC1_MOTCONF,ADR_TMC1_CPR,ADR_TMC1_ENCA,ADR_TMC1_OFFSETFLUX,ADR_TMC1_TORQUE_P,ADR_TMC1_TORQUE_I,ADR_TMC1_FLUX_P,ADR_TMC1_FLUX_I});
	}else if(addr == 2){
		this->cspin = SPI1_SS2_Pin;
		this->csport = SPI1_SS2_GPIO_Port;
		this->spi = &HSPIDRV;
		//TODO addr
	}else if(addr == 3){
		this->cspin = SPI1_SS3_Pin;
		this->csport = SPI1_SS3_GPIO_Port;
		this->spi = &HSPIDRV;
		//TODO addr
	}
}

uint8_t TMC4671::getAddress(){
	return this->address;
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
		if(!encoderInitialized){
			return false;
		}
	}

	return true;
}

bool TMC4671::initialize(){
	active = true;
	if(state == TMC_ControlState::uninitialized){
		state = TMC_ControlState::Init_wait;
	}
	// Check if a TMC4671 is active and replies correctly
	writeReg(1, 0);
	if(readReg(0) != 0x34363731){// 4671
		pulseErrLed();
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
		this->spi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
		HAL_SPI_Init(this->spi);
		oldTMCdetected = true;
	}

	// Write main constants
	setMotionMode(MotionMode::stop);
	setUdUq(0, 0);
	setPwm(0,conf.pwmcnt,conf.bbmL,conf.bbmH); // Enable FOC @ 25khz
	setMotorType(conf.motconf.motor_type,conf.motconf.pole_pairs);
	setPhiEtype(conf.motconf.phiEsource);
	setup_HALL(hallconf); // Enables hall filter and masking

	initAdc(conf.mdecA,conf.mdecB,conf.mclkA,conf.mclkB);
	//setAdcOffset(conf.adc_I0_offset, conf.adc_I1_offset);
	setAdcScale(conf.adc_I0_scale, conf.adc_I1_scale);

	// Calibrate ADC every time for now
	calibrateAdcOffset();

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


	//setPos(0);

	setPids(curPids); // Write basic pids
	// Enable driver
	updateReg(0x1A,7,0xff,0); // Enable FOC PWM
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_SET);
	writeReg(0x64, 0); // No flux/torque
	setStatusMask(0); // Disable status output by default.

	setEncoderType(conf.motconf.enctype);


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
	float r = thermistor_R2 * (((float)43252 / (float)adcval) -1.0); //43252 equivalent ADC count if it was 3.3V and not 2.5V

	// Beta
	r = (1.0 / 298.15) + log(r / thermistor_R) / thermistor_Beta;
	r = 1.0 / r;
	r -= 273.15;
	return r;

}

void TMC4671::update(){
	// Main state machine
	switch(this->state){


	case TMC_ControlState::Running:
	{
		// Check status, Temps, Everything alright?
		uint32_t tick = HAL_GetTick();
		if(tick - lastStatTime > 2000){ // Every 2s
			lastStatTime = tick;

			// Get enable input
			bool tmc_en = (readReg(0x76) >> 15) & 0x01;
			if(!tmc_en && active){ // Hardware emergency
				this->estopTriggered = true;
				state = TMC_ControlState::HardError;
			}

			// Temperature sense
			#ifdef TMCTEMP
			float temp = getTemp();
			if(temp > temp_limit){
				state = TMC_ControlState::OverTemp;
				pulseErrLed();
			}
			#endif

		}
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
		this->stop();
		state = TMC_ControlState::HardError; // Block
	break;

	default:

	break;
	}

	// Optional update methods for safety

	if(!hasPower()){ // low voltage or overvoltage
		state = TMC_ControlState::Init_wait;
		initialized = false;
		pulseErrLed();
		if(!emergency && HAL_GetTick() - initTime > 100)
			emergencyStop();
		else
			initTime = HAL_GetTick();
	}


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

void TMC4671::setTargetPos(int32_t pos){
	if(curMotionMode != MotionMode::position){
		setMotionMode(MotionMode::position);
	}
	writeReg(0x68,pos);
}
int32_t TMC4671::getTargetPos(){

	return readReg(0x68);
}


void TMC4671::setTargetVelocity(int32_t vel){
	if(curMotionMode != MotionMode::velocity){
		setMotionMode(MotionMode::velocity);
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

	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setPhiE_ext(0);
	setUdUq(power, 0);
	//int32_t pos = getPos();

	setPhiEtype(PhiE::ext);

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
	setMotionMode(MotionMode::uqudext);

	HAL_Delay(100);
	setPhiE_ext(0x3fff);
	int16_t phiE_abn = readReg(phiEreg)>>16;
	HAL_Delay(250);
	int16_t phiE_abn_old = 0;
	int16_t c = 0;
	while(abs(phiE_abn - phiE_abn_old) > 100 && c++ < 50){
		refreshWatchdog();
		phiE_abn_old = phiE_abn;
		phiE_abn=readReg(phiEreg)>>16;
		HAL_Delay(100);
	}

	//Write offset
	//int16_t phiE_abn = readReg(0x2A)>>16;
	abnconf.phiEoffset = 0x3fff-phiE_abn;
	updateReg(phiEoffsetReg, abnconf.phiEoffset, 0xffff, 16);

	setUdUq(0, 0);
	setPhiE_ext(0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode);
	//setPos(pos+getPos());
	setPos(0);
}

// Rotates motor to find min and max values of the encoder
void TMC4671::calibrateAenc(){
	// Rotate and measure min/max

	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	//int32_t pos = getPos();
	PosSelection possel = this->conf.motconf.pos_sel;
	setPosSel(PosSelection::PhiE_openloop);
	setPos(0);

	uint32_t minVal_0 = 0xffff,	minVal_1 = 0xffff,	minVal_2 = 0xffff;
	uint32_t maxVal_0 = 0,	maxVal_1 = 0,	maxVal_2 = 0;
	int32_t minpos = -0x8fff/MAX(1,MIN(this->aencconf.cpr/4,20)), maxpos = 0x8fff/MAX(1,MIN(this->aencconf.cpr/4,20));
	uint32_t speed = MAX(1,20/MAX(1,this->aencconf.cpr/5));
	runOpenLoop(3000, 0, speed, 100);

	uint8_t stage = 0;
	int32_t poles = conf.motconf.pole_pairs;
	int32_t initialDirPos = 0;
	while(stage != 3){
		HAL_Delay(2);
		refreshWatchdog(); // Don't let the dog get any sleep
		if(getPos() > maxpos*poles && stage == 0){
			runOpenLoop(bangInitPower, 0, -speed, 100);
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
			runOpenLoop(0, 0, 0, 1000);
			HAL_Delay(250);
			// Zero aenc
			writeReg(0x41, 0);
			initialDirPos = readReg(0x41);
			runOpenLoop(bangInitPower, 0, speed, 100);
			stage = 2;
		}else if(getPos() > 0 && stage == 2){
			stage = 3;
			runOpenLoop(0, 0, 0, 1000);
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
	setMotionMode(lastmode);
	setPosSel(possel);
	setPos(0);
}

/*
 * Steps the motor a few times to check if the encoder follows correctly
 */
bool TMC4671::checkEncoder(){

	uint8_t phiEreg = 0;
	if(conf.motconf.enctype == EncoderType_TMC::abn){
		phiEreg = 0x2A;
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		phiEreg = 0x46;
	}

	bool result = true;
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setUdUq(bangInitPower, 0);
	setPhiEtype(PhiE::ext);
	setMotionMode(MotionMode::uqudext);

	//int16_t phiE_enc_start = (int16_t)(readReg(phiEreg)>>16);
	int16_t phiE_enc = 0;

	for(int16_t angle = -0x3fff;angle<0x3fff;angle+=0x0fff){
		uint16_t c = 0;
		setPhiE_ext(angle);
		HAL_Delay(100);
		phiE_enc = (int16_t)(readReg(phiEreg)>>16);
		int16_t err = abs(phiE_enc - angle);
		// Wait more
		while(err > 6000 && c++ < 500){
			phiE_enc = (int16_t)(readReg(phiEreg)>>16);
			err = abs(phiE_enc - angle);
			HAL_Delay(10);
			refreshWatchdog();
		}
		// still high difference?
		if(err > 8000){
			result = false;
			break;
		}
	}


	setUdUq(0, 0);
	setPhiE_ext(0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode);

	if(result){
		encoderInitialized = true;
	}
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
void TMC4671::calibrateAdcOffset(){

	uint16_t measuretime_idle = 500;
	uint32_t measurements_idle = 0;
	uint64_t totalA=0;
	uint64_t totalB=0;

	writeReg(0x03, 0); // Read raw adc
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();


	uint16_t lastrawA=conf.adc_I0_offset, lastrawB=conf.adc_I1_offset;

	//pulseClipLed(); // Turn on led
	// Disable drivers and measure many samples of zero current
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_RESET);
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
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_SET);
	uint32_t offsetAidle = totalA / (measurements_idle);
	uint32_t offsetBidle = totalB / (measurements_idle);
	totalA = 0, totalB=0;
	conf.adc_I0_offset = offsetAidle;
	conf.adc_I1_offset = offsetBidle;
	setAdcOffset(conf.adc_I0_offset, conf.adc_I1_offset);
	// ADC Offsets should now be close to perfect

	setPhiEtype(lastphie);
	setMotionMode(lastmode);
}


void TMC4671::ABN_init(){
	if(this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC){
		encstate = ENC_InitState::OK;
		return;
	}
	switch(encstate){
		case ENC_InitState::uninitialized:
			setPosSel(PosSelection::PhiM_abn); // Mechanical Angle
			writeReg(0x26, abnconf.cpr); // we need cpr to be set first
			encstate = ENC_InitState::estimating;
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
			if(hasPower() && !encoderInitialized){
				bangInitEnc(bangInitPower);
				encstate = ENC_InitState::checking;
			}
		break;

		case ENC_InitState::checking:
			if(checkEncoder()){
				encstate = ENC_InitState::OK;
				encoderInitialized = true;
				setPhiEtype(PhiE::abn);
				state = TMC_ControlState::Running;
				enc_retry = 0;

			}else{
				if(enc_retry++ > enc_retry_max){
					state = TMC_ControlState::HardError;
				}
				encstate = ENC_InitState::uninitialized; // Retry
			}
		break;
		case ENC_InitState::OK:
			state = TMC_ControlState::Running;
			break;
	}
}

void TMC4671::AENC_init(){
	if(this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC){
		encstate = ENC_InitState::OK;
		return;
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
				encoderInitialized = true;
				setPhiEtype(PhiE::aenc);
				state = TMC_ControlState::Running;
				enc_retry = 0;

			}else{
				if(enc_retry++ > enc_retry_max){
					state = TMC_ControlState::HardError;
				}
				encstate = ENC_InitState::uninitialized; // Retry
			}
		break;
		case ENC_InitState::OK:
			state = TMC_ControlState::Running;
			break;
	}
}

/*
 * Changes the encoder type and calls init methods for the encoder types.
 * Setup the specific parameters (abnconf, aencconf...) first.
 */
void TMC4671::setEncoderType(EncoderType_TMC type){
	this->conf.motconf.enctype = type;

	if(type == EncoderType_TMC::abn){

		// Not initialized if cpr not set
		if(this->abnconf.cpr == 0){
			return;
		}

		state = TMC_ControlState::ABN_init;
		encstate = ENC_InitState::uninitialized;

	// Todo align aenc
	// SinCos encoder
	}else if(type == EncoderType_TMC::sincos){
		state = TMC_ControlState::AENC_init;
		encstate = ENC_InitState::uninitialized;
		this->aencconf.uvwmode = false; // sincos mode

	// Analog UVW encoder
	}else if(type == EncoderType_TMC::uvw){
		state = TMC_ControlState::AENC_init;
		encstate = ENC_InitState::uninitialized;
		this->aencconf.uvwmode = true; // uvw mode

	}else if(type == EncoderType_TMC::hall){ // Hall sensor. Just trust it
		state = TMC_ControlState::Running;
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

void TMC4671::setMotionMode(MotionMode mode){
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


void TMC4671::runOpenLoop(uint16_t ud,uint16_t uq,int32_t speed,int32_t accel){
	setPhiEtype(PhiE::openloop);
	setMotionMode(MotionMode::uqudext);
	setUdUq(ud,uq);
	setOpenLoopSpeedAccel(speed, accel);
}

void TMC4671::setUdUq(int16_t ud,int16_t uq){
	writeReg(0x24, ud | (uq << 16));
}

void TMC4671::stop(){
	// Stop driver instantly
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_RESET);
	active = false;
}
void TMC4671::start(){
	if(!initialized){
		initialize();
	}else{
		if(emergency){
			// Reenable foc
			updateReg(0x1A,7,0xff,0);
			emergency = false;
		}
		HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_SET);
		active = true;
	}

}

void TMC4671::emergencyStop(){
	updateReg(0x1A,1,0xff,0); // Short low side for instant stop
	emergency = true;
}

/*
 * Sets a torque in positive or negative direction
 * For ADC linearity reasons under 25000 is recommended
 */
void TMC4671::turn(int16_t power){
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

	bool reinit = cpr != abnconf.cpr;
	this->abnconf.cpr = cpr;
	this->aencconf.cpr = cpr;
	writeReg(0x26, abnconf.cpr); //ABN
	writeReg(0x40, aencconf.cpr); //AENC

	if(reinit && (this->conf.motconf.phiEsource == PhiE::abn))
		bangInitEnc(this->bangInitPower);
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
		setMotionMode(MotionMode::torque);
	}
	updateReg(0x64,torque,0xffff,16);
}
int16_t TMC4671::getTorque(){
	return readReg(0x64) >> 16;
}

void TMC4671::setFlux(int16_t flux){
	if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque);
	}
	updateReg(0x64,flux,0xffff,0);
}
int16_t TMC4671::getFlux(){
	return readReg(0x64) && 0xffff;
}
void TMC4671::setFluxTorque(int16_t flux, int16_t torque){
	if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque);
	}
	writeReg(0x64, (flux & 0xffff) | (torque << 16));
}

void TMC4671::setFluxTorqueFF(int16_t flux, int16_t torque){
	if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque);
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
	int32_t pos = getPos();
	setPos(0);
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	updateReg(0x25, 0,0x1000,12); // Set dir normal
	setPhiE_ext(0);
	setUdUq(bangInitPower,0);
	setMotionMode(MotionMode::uqudext);
	setPhiEtype(PhiE::ext);
	int16_t phiE_abn = readReg(0x2A)>>16;
	int16_t phiE_abn_old = 0;
	int16_t rcount=0,c = 0; // Count how often direction was in reverse
	uint16_t highcount = 0; // Count high state of n pulse for polarity estimation

	// Rotate a bit
	for(uint16_t p = 0;p<0x0fff;p+=0x2f){
		setPhiE_ext(p);
		HAL_Delay(10);
		refreshWatchdog(); // Keep board from resetting
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

	setUdUq(0, 0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode);

	bool npol = highcount > c/2;
	abnconf.rdir = rcount > c/2;
//	if(npol != abnconf.npol) // Invert dir if polarity was reversed TODO correct? likely wrong at the moment
//		abnconf.rdir = !abnconf.rdir;

	abnconf.apol = npol;
	abnconf.bpol = npol;
	abnconf.npol = npol;
}


void TMC4671::setStatusMask(uint32_t mask){
	writeReg(0x7D, mask);
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

int32_t TMC4671::getActualCurrent(){
	uint32_t tfluxa = readReg(0x69);
	//int16_t af = (tfluxa & 0xffff);
	int16_t at = (tfluxa >> 16);
	return (at);
}

__attribute__((optimize("-Ofast")))
uint32_t TMC4671::readReg(uint8_t reg){
	uint8_t req[5] = {(uint8_t)(0x7F & reg),0,0,0,0};
	uint8_t tbuf[5];
	// 500ns delay after sending first byte

	//__disable_irq();
	while(this->spi_busy){} // wait if a tx was just started
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	//HAL_SPI_Transmit(this->spi,req,1,SPITIMEOUT); // pause
	//HAL_SPI_Receive(this->spi,tbuf,4,SPITIMEOUT);
	HAL_SPI_TransmitReceive(this->spi,req,tbuf,5,SPITIMEOUT);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);
	//__enable_irq();

	uint32_t ret;
	memcpy(&ret,tbuf+1,4);
	ret = __REV(ret);
	return ret;
}

__attribute__((optimize("-Ofast")))
void TMC4671::writeReg(uint8_t reg,uint32_t dat){

	// wait until ready
	while(this->spi_busy){}
	this->spi_busy = true;

	spi_buf[0] = (uint8_t)(0x80 | reg);
	dat =__REV(dat);
	memcpy(spi_buf+1,&dat,4);

	//__disable_irq();
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_Transmit_DMA(this->spi,spi_buf,5);
	//HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);
	//__enable_irq();
}

void TMC4671::updateReg(uint8_t reg,uint32_t dat,uint32_t mask,uint8_t shift){

	uint32_t t = readReg(reg) & ~(mask << shift);
	t |= ((dat & mask) << shift);
	writeReg(reg, t);
}
void TMC4671::SpiTxCplt(SPI_HandleTypeDef *hspi){
	if(hspi == this->spi && this->spi_busy){
		this->spi_busy = false;
		HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET); // unselect
	}
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

	val |= (this->hallconf.direction & 0x01) << 8;
	val |= (this->hallconf.interpolation & 0x01) << 9;

	val |= (this->curPids.sequentialPI & 0x01) << 10;

	return val;
}

void TMC4671::restoreEncHallMisc(uint16_t val){

	this->abnconf.apol = (val) & 0x01;
	this->abnconf.bpol = this->abnconf.apol;
	this->abnconf.npol = this->abnconf.apol;
	this->abnconf.rdir = (val>>1) & 0x01; // Direction
	this->abnconf.ab_as_n = (val>>2) & 0x01;
	this->hallconf.direction = (val>>8) & 0x01;
	this->hallconf.interpolation = (val>>9) & 0x01;

	this->curPids.sequentialPI = (val>>10) & 0x01;
}


ParseStatus TMC4671::command(ParsedCommand* cmd,std::string* reply){
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
			this->bangInitEnc(5000);
		}else if(cmd->type ==CMDtype:: set){
			this->bangInitEnc(cmd->val);
		}else{
			return ParseStatus::OK_CONTINUE;
		}
		if(this->checkEncoder()){
			*reply+=">Aligned";
		}else{
			*reply+="Encoder error";
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
			*reply+=std::to_string(getActualCurrent());
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
	}else if(cmd->cmd == "phiesrc"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->getPhiEtype());
		}else if(cmd->type == CMDtype::set){
			this->setPhiEtype((PhiE)cmd->val);
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

	}else if(cmd->cmd == "encdir"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->abnconf.rdir);
		}else if(cmd->type == CMDtype::set){
			this->abnconf.rdir = cmd->val != 0;
			this->setup_ABN_Enc(this->abnconf);
		}

	}else if(cmd->cmd == "tmctemp"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->getTemp());
		}

	}else{
		flag = ParseStatus::NOT_FOUND;
	}
	return flag;
}

