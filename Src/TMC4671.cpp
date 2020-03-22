/*
 * TMC4671.cpp
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#include "TMC4671.h"
#include "ledEffects.h"
#include "voltagesense.h"

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
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC1_MOTCONF,ADR_TMC1_PPR,ADR_TMC1_ENCA,ADR_TMC1_ENCOFFSET});
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
	uint16_t ppr = this->abnconf.ppr;
	int16_t encoffset = this->abnconf.phiEoffset;
	// Save flash
	Flash_Write(flashAddrs.mconf, mconfint);
	Flash_Write(flashAddrs.ppr, ppr);
	Flash_Write(flashAddrs.encOffset,encoffset);
	Flash_Write(flashAddrs.offsetFlux,maxOffsetFlux);
	Flash_Write(flashAddrs.encA,encodeEncHallMisc());
}

void TMC4671::restoreFlash(){
	uint16_t mconfint;
	uint16_t ppr = 0;
	uint16_t encoffset;

	// Read flash
	if(Flash_Read(flashAddrs.mconf, &mconfint))
		this->conf.motconf = TMC4671::decodeMotFromInt(mconfint);

	if(Flash_Read(flashAddrs.ppr, &ppr))
		this->abnconf.ppr = ppr;

	if(Flash_Read(flashAddrs.encOffset, &encoffset))
		this->abnconf.phiEoffset = (int16_t)encoffset;

	Flash_Read(flashAddrs.offsetFlux, (uint16_t*)&this->maxOffsetFlux);

	uint16_t encval;
	if(Flash_Read(flashAddrs.encA, &encval)){
		restoreEncHallMisc(encval);
	}

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
		if(abnconf.ppr == 0){
			return false;
		}
		if(!abnconf.initialized){
			return false;
		}
	}

	return true;
}

bool TMC4671::initialize(){
	active = true;
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
	setPwm(0,conf.pwmcnt,conf.bbmL,conf.bbmH); // Enable FOC @ 25khz
	setMotorType(conf.motconf.motor_type,conf.motconf.pole_pairs);
	setPhiEtype(conf.motconf.phiEsource);

	initAdc(conf.mdecA,conf.mdecB,conf.mclkA,conf.mclkB);
	setAdcOffset(conf.adc_I0_offset, conf.adc_I1_offset);
	setAdcScale(conf.adc_I0_scale, conf.adc_I1_scale);

	// brake res failsafe TODO
	setBrakeLimits(62000,63000);

	bool npol = findABNPol();
	abnconf.apol = npol;
	abnconf.bpol = npol;
	abnconf.npol = npol;

	setPos(0);

	setPids(curPids); // Write basic pids
	// Enable driver
	updateReg(0x1A,7,0xff,0); // Enable FOC PWM
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_SET);
	writeReg(0x64, 0); // No flux/torque
	setStatusMask(0); // Disable status output by default.

	if(this->conf.motconf.phiEsource == PhiE::abn){
		// Not initialized if ppr not set
		if(this->abnconf.ppr == 0){
			return false;
		}
		setup_ABN_Enc(this->abnconf);
	}
	//TODO
	// Initialize Encoder/Hall

	// Home?
	// Run in direction of N pulse. Enable flag/interrupt
	//runOpenLoop(3000, 0, 5, 100);
	initialized = true;
	initTime = HAL_GetTick();
	return initialized;

}

void TMC4671::update(){
	// Optional update methods for safety

	if(!hasPower()){ // low voltage or overvoltage
		initialized = false;
		initTime = HAL_GetTick();
		pulseErrLed();
		if(!emergency)
			emergencyStop();
	}

	if(!initialized && active && hasPower()){
		if(HAL_GetTick() - initTime > (emergency ? 5000 : 1000)){
			emergency = false;
			initialize();
		}
	}

	if(!isSetUp()){
		pulseErrLed();
		initialized = false;
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


void TMC4671::setPhiE_ext(int16_t phiE){
	writeReg(0x1C, phiE);
}

/*
 * Aligns ABN encoders by forcing an angle with high current and calculating the offset
 */
void TMC4671::bangInitABN(int16_t power){
	if(!hasPower()){
		return;
	}
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setPhiE_ext(0);
	setUdUq(power, 0);

	setPhiEtype(PhiE::ext);
	int32_t encpos = readReg(0x28);
	writeReg(0x28,0); //Zero encoder
	updateReg(0x29, 0, 0xffff, 16); // Set phiE offset to zero
	setMotionMode(MotionMode::uqudext);

	HAL_Delay(100);
	setPhiE_ext(4096);
	HAL_Delay(250);
	//Write offset
	int16_t phiE_abn = readReg(0x2A)>>16;
	abnconf.phiEoffset = 4096-phiE_abn;
	updateReg(0x29, abnconf.phiEoffset, 0xffff, 16);

	setUdUq(0, 0);
	setPhiE_ext(0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode);
	writeReg(0x28,encpos+(int32_t)readReg(0x28)); //Restore encoder
}


bool TMC4671::checkABN(){

	bool result = true;
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setUdUq(abnconf.bangInitPower, 0);
	setPhiEtype(PhiE::ext);
	setMotionMode(MotionMode::uqudext);

	int16_t phiE_abn_start = readReg(0x2A)>>16;

	for(int16_t angle = -0x3fff;angle<0x3fff;angle+=0x0fff){
		setPhiE_ext(angle);
		HAL_Delay(100);
		int16_t phiE_abn = readReg(0x2A)>>16;
		// High difference?
		uint16_t err = abs(phiE_abn - angle);
		if(err > 4000){
			result = false;
			break;
		}
	}
	// Encoder did not move at all
	int16_t phiE_abn = readReg(0x2A)>>16;
	if(phiE_abn_start ==  phiE_abn){
		result = false;
	}

	setUdUq(0, 0);
	setPhiE_ext(0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode);

	if(result){
		abnconf.initialized = true;
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
			(encconf.latch_on_N << 4) |
			(encconf.rdir << 5));

	writeReg(0x25, abnmode);
	writeReg(0x26, encconf.ppr);
	writeReg(0x29, (encconf.phiEoffset << 16) | encconf.phiMoffset);
	writeReg(0x28,0); //Zero encoder
	//conf.motconf.phiEsource = PhiE::abn;
	this->setPhiEtype(PhiE::abn);
	if(hasPower() && !encconf.initialized){
		bangInitABN(encconf.bangInitPower);
		// Check if aligned
		if(checkABN()){
			abnconf.initialized = true;
		}
	}
}
void TMC4671::setup_HALL(TMC4671HALLConf hallconf){
	this->hallconf = hallconf;

	uint32_t hallmode =
			hallconf.polarity |
			hallconf.interpolation << 8 |
			hallconf.direction << 12 |
			(hallconf.blank & 0xfff) << 16;
	writeReg(0x33, hallmode);
	// Positions
	uint32_t posA = hallconf.pos0 | hallconf.pos60 << 16;
	writeReg(0x34, posA);
	uint32_t posB = hallconf.pos120 | hallconf.pos180 << 16;
	writeReg(0x35, posB);
	uint32_t posC = hallconf.pos240 | hallconf.pos300 << 16;
	writeReg(0x36, posC);
	uint32_t phiOffsets = hallconf.phiMoffset | hallconf.phiEoffset << 16;
	writeReg(0x37, phiOffsets);
	writeReg(0x38, hallconf.dPhiMax);

	//conf.motconf.phiEsource = PhiE::hall;
}


void TMC4671::calibrateAdcScale(){

	// Estimate ADC Parameters by running openloop
	uint16_t measuretime_run = 500;

	uint32_t measurements_run = 0;

	uint16_t maxA=0,minA=0xffff;
	uint16_t maxB=0,minB=0xffff;
	writeReg(0x03, 0); // Read raw adc
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();

	runOpenLoop(5000, 0, -5, 100);
	pulseClipLed(); // Turn on led


	uint32_t tick = HAL_GetTick();
	uint16_t lastrawA=conf.adc_I0_offset, lastrawB=conf.adc_I1_offset;
	while((tick + measuretime_run) > HAL_GetTick()){
		if((tick + (3*measuretime_run/4)) == HAL_GetTick()){
			setOpenLoopSpeedAccel(-5, 100);
		}else if((tick + measuretime_run/4) == HAL_GetTick()){
			setOpenLoopSpeedAccel(5, 100);
		}
		uint32_t adcraw = readReg(0x02);
		uint16_t rawA = adcraw & 0xffff;
		uint16_t rawB = (adcraw >> 16) & 0xffff;

		// glitch removal
		if(abs(lastrawA-rawA) < 3000 && abs(lastrawB-rawB) < 3000){
			measurements_run++;
			maxA = MAX(rawA,maxA);
			maxB = MAX(rawB,maxB);
			minA = MIN(rawA,minA);
			minB = MIN(rawB,minB);

			lastrawA = rawA;
			lastrawB = rawB;
		}

	}
	setOpenLoopSpeedAccel(0, 100);

	conf.adc_I1_scale *= (float(maxA - minA)+1) / float((maxB-minB)+1);
	setAdcScale(conf.adc_I0_scale, conf.adc_I1_scale);

	setPhiEtype(lastphie);
	setMotionMode(lastmode);
}

void TMC4671::calibrateAdcOffset(){

	// Estimate ADC Parameters by running openloop
	uint16_t measuretime_idle = 500;
	uint32_t measurements_idle = 0;
	uint64_t totalA=0;
	uint64_t totalB=0;

	writeReg(0x03, 0); // Read raw adc
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();

	uint16_t lastrawA=conf.adc_I0_offset, lastrawB=conf.adc_I1_offset;

	pulseClipLed(); // Turn on led
	// Disable drivers and measure many samples of zero current
	HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin,GPIO_PIN_RESET);
	uint32_t tick = HAL_GetTick();
	while(tick + measuretime_idle > HAL_GetTick()){ // Measure idle
		uint32_t adcraw = readReg(0x02);
		uint16_t rawA = adcraw & 0xffff;
		uint16_t rawB = (adcraw >> 16) & 0xffff;
		// Signflip filter
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

void TMC4671::setPhiEtype(PhiE type){
	conf.motconf.phiEsource = type;
	writeReg(0x52, (uint8_t)type & 0xff);
}
PhiE TMC4671::getPhiEtype(){
	return PhiE(readReg(0x52) & 0xff);
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

void TMC4671::turn(int16_t power){
	//
	if(feedforward){
		setFluxTorque(idleFlux-clip<int32_t,int16_t>(abs(power),0,maxOffsetFlux), power/2);
		setFluxTorqueFF(0, power/2);
	}else{
		setFluxTorque(idleFlux-clip<int32_t,int16_t>(abs(power),0,maxOffsetFlux), power);
	}
	//setTorque(power);
}

int32_t TMC4671::getPos(){

	return readReg(0x6B);
}
void TMC4671::setPos(int32_t pos){
	//writeReg(0x27, pos);
	writeReg(0x6B, pos);
}
uint32_t TMC4671::getPosCpr(){
	return conf.motconf.pole_pairs << 16; // PhiE = 0xffff
}

uint32_t TMC4671::getPpr(){
	return abnconf.ppr;
}
void TMC4671::setPpr(uint32_t cpr){
	bool reinit = cpr != abnconf.ppr;
	this->abnconf.ppr = cpr;
	writeReg(0x26, abnconf.ppr);

	if(reinit)
		bangInitABN(this->abnconf.bangInitPower);
}

uint32_t TMC4671::encToPos(uint32_t enc){
	return enc*(0xffff / abnconf.ppr)*(conf.motconf.pole_pairs);
}
uint32_t TMC4671::posToEnc(uint32_t pos){
	return pos/((0xffff / abnconf.ppr)*(conf.motconf.pole_pairs)) % abnconf.ppr;
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
		updateReg(0x63, 1, 0x1, 31);
	}else{
		feedforward = false;
	}
}

void TMC4671::setMotorType(MotorType motor,uint16_t poles){
	conf.motconf.motor_type = motor;
	conf.motconf.pole_pairs = poles;
	uint32_t mtype = poles | ( ((uint8_t)motor&0xff) << 16);
	writeReg(0x1B, mtype);
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
 * Detect encoder polarity by blocking motor and checking N channel.
 */
bool TMC4671::findABNPol(){
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();

	setUdUq(3000, 0);
	setMotionMode(MotionMode::uqudext);
	setPhiEtype(PhiE::ext);
	uint16_t count=0;
	uint16_t highcount = 0;
	for(uint16_t p = 0;p<0x0fff;p+=0xf){
		HAL_Delay(1);
		count++;
		if((readReg(0x76) & 0x04) >> 2){
			highcount++;
		}
	}

	setUdUq(0, 0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode);

	bool npol = highcount > count/2;
//	abnconf.apol = npol;
//	abnconf.bpol = npol;
//	abnconf.npol = npol;
//	setup_ABN_Enc(this->abnconf);
	return npol;
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



uint32_t TMC4671::readReg(uint8_t reg){

	uint8_t req[5] = {(uint8_t)(0x7F & reg),0,0,0,0};
	uint8_t tbuf[5];
	// 500ns delay after sending first byte already satisfied by STM
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(this->spi,req,tbuf,5,SPITIMEOUT);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);

	uint32_t ret;
	memcpy(&ret,tbuf+1,4);
	ret = __REV(ret);
	return ret;
}

void TMC4671::writeReg(uint8_t reg,uint32_t dat){

	uint8_t req[5] = {(uint8_t)(0x80 | reg),0,0,0,0};
	dat =__REV(dat);
	memcpy(req+1,&dat,4);

	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_RESET);
	HAL_SPI_Transmit(this->spi,req,5,SPITIMEOUT);
	HAL_GPIO_WritePin(this->csport,this->cspin,GPIO_PIN_SET);

}

void TMC4671::updateReg(uint8_t reg,uint32_t dat,uint32_t mask,uint8_t shift){

	uint32_t t = readReg(reg) & ~(mask << shift);
	t |= ((dat & mask) << shift);
	writeReg(reg, t);
}

TMC4671MotConf TMC4671::decodeMotFromInt(uint16_t val){
	// 0-2: MotType 3-5: PhiE source 6-15: Poles
	TMC4671MotConf mot;
	mot.motor_type = MotorType(val & 0x7);
	mot.phiEsource = PhiE( (val >> 3) & 0x7);
	mot.pole_pairs = val >> 6;
	return mot;
}
uint16_t TMC4671::encodeMotToInt(TMC4671MotConf mconf){
	uint16_t val = (uint8_t)mconf.motor_type & 0x7;
	val |= ((uint8_t)mconf.phiEsource & 0x7) << 3;
	val |= (mconf.pole_pairs & 0x3FF) << 6;
	return val;
}

uint16_t TMC4671::encodeEncHallMisc(){
	uint16_t val = 0;
	val |= (this->abnconf.npol) & 0x01;
	val |= (this->abnconf.rdir <<1) & 0x01; // Direction
	val |= (this->abnconf.ab_as_n <<2) & 0x01;

	val |= (this->hallconf.direction << 8) & 0x01;
	val |= (this->hallconf.interpolation << 9) & 0x01;

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
}


bool TMC4671::command(ParsedCommand* cmd,std::string* reply){
	bool flag = true;
	if(cmd->cmd == "mtype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.motconf.motor_type);
		}else if(cmd->type == CMDtype::set && (uint8_t)cmd->type < (uint8_t)MotorType::ERR){
			this->setMotorType((MotorType)cmd->val, this->conf.motconf.pole_pairs);
		}else{
			*reply+="NONE=0,DC=1,STEPPER=2,BLDC=3";
		}

	}else if(cmd->cmd == "encalign"){
		if(cmd->type == CMDtype::get){
			this->bangInitABN(5000);
		}else if(cmd->type ==CMDtype:: set){
			this->bangInitABN(cmd->val);
		}else{
			return false;
		}
		if(this->checkABN()){
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

	}else if(cmd->cmd == "help"){
		*reply += "TMC4671 commands:\n"
				"mtype,encalign,poles,phiesrc,reg,fluxoffset\n"
				"torqueP,torqueI,fluxP,fluxI\n";
	}else{
		flag = false;
	}
	return flag;
}
