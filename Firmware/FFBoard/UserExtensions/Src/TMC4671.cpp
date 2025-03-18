/*
 * TMC4671.cpp
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#include "TMC4671.h"
#ifdef TMC4671DRIVER
#include "ledEffects.h"
#include "voltagesense.h"
//#include "stm32f4xx_hal_spi.h"
#include <math.h>
#include <assert.h>
#include "ErrorHandler.h"
#include "cpp_target_config.h"
#define MAX_TMC_DRIVERS 3

ClassIdentifier TMC_1::info = {
	.name = "TMC4671 (CS 1)",
	.id=CLSID_MOT_TMC0, // 1
};


bool TMC_1::isCreatable() {
	return motor_spi.isPinFree(*motor_spi.getCsPin(0));
}


ClassIdentifier TMC_2::info = {
	.name = "TMC4671 (CS 2)" ,
	.id=CLSID_MOT_TMC1,
};


bool TMC_2::isCreatable() {
	return motor_spi.isPinFree(*motor_spi.getCsPin(1));
}




ClassIdentifier TMC4671::info = {
	.name = "TMC4671" ,
	.id=CLSID_MOT_TMC0,
};


TMC4671::TMC4671(SPIPort& spiport,OutputPin cspin,uint8_t address) :
		CommandHandler("tmc", CLSID_MOT_TMC0,address-1), SPIDevice{motor_spi,cspin},Thread("TMC", TMC_THREAD_MEM, TMC_THREAD_PRIO)
{
	CommandHandler::setCommandsEnabled(false);
	setAddress(address);
	registerCommands();
	spiConfig.peripheral = motor_spi.getPortHandle()->Init;
	spiConfig.peripheral.Mode = SPI_MODE_MASTER;
	spiConfig.peripheral.Direction = SPI_DIRECTION_2LINES;
	spiConfig.peripheral.DataSize = SPI_DATASIZE_8BIT;
	spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH;
	spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
	spiConfig.peripheral.NSS = SPI_NSS_SOFT;
	spiConfig.peripheral.BaudRatePrescaler = spiPort.getClosestPrescaler(10e6).first; // 10MHz
	spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_MSB;
	spiConfig.peripheral.TIMode = SPI_TIMODE_DISABLE;
	spiConfig.peripheral.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	spiConfig.cspol = true;

#ifdef TMC4671_SPI_DATA_IDLENESS
	spiConfig.peripheral.MasterInterDataIdleness = TMC4671_SPI_DATA_IDLENESS;
#endif

	spiPort.takeSemaphore();
	spiPort.configurePort(&spiConfig.peripheral);
	spiPort.giveSemaphore();

	this->restoreFlash();
	CommandHandler::setCommandsEnabled(true);
}


TMC4671::~TMC4671() {
	enablePin.reset();
	//recordSpiAddrUsed(0);
}


const ClassIdentifier TMC4671::getInfo() {

	return info;
}


void TMC4671::setAddress(uint8_t address){
	if (address == 1){
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC1_MOTCONF, ADR_TMC1_CPR, ADR_TMC1_ENCA, ADR_TMC1_OFFSETFLUX, ADR_TMC1_TORQUE_P, ADR_TMC1_TORQUE_I, ADR_TMC1_FLUX_P, ADR_TMC1_FLUX_I,ADR_TMC1_ADC_I0_OFS,ADR_TMC1_ADC_I1_OFS,ADR_TMC1_ENC_OFFSET,ADR_TMC1_PHIE_OFS,ADR_TMC1_TRQ_FILT});
	}else if (address == 2)
	{
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC2_MOTCONF, ADR_TMC2_CPR, ADR_TMC2_ENCA, ADR_TMC2_OFFSETFLUX, ADR_TMC2_TORQUE_P, ADR_TMC2_TORQUE_I, ADR_TMC2_FLUX_P, ADR_TMC2_FLUX_I,ADR_TMC2_ADC_I0_OFS,ADR_TMC2_ADC_I1_OFS,ADR_TMC2_ENC_OFFSET,ADR_TMC2_PHIE_OFS,ADR_TMC2_TRQ_FILT});
	}else if (address == 3)
	{
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC3_MOTCONF, ADR_TMC3_CPR, ADR_TMC3_ENCA, ADR_TMC3_OFFSETFLUX, ADR_TMC3_TORQUE_P, ADR_TMC3_TORQUE_I, ADR_TMC3_FLUX_P, ADR_TMC3_FLUX_I,ADR_TMC3_ADC_I0_OFS,ADR_TMC3_ADC_I1_OFS,ADR_TMC3_ENC_OFFSET,ADR_TMC3_PHIE_OFS,ADR_TMC3_TRQ_FILT});
	}
	//this->setAxis((char)('W'+address));
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
	Flash_Write(flashAddrs.encOffset,(uint16_t)abnconf.posOffsetFromIndex);

	// If encoder is ABN and uses index save the last configured offset
//	if(this->conf.motconf.enctype == EncoderType_TMC::abn && this->abnconf.useIndex && encoderAligned){
//		Flash_Write(flashAddrs.phieOffset, abnconf.phiEoffset);
//	}

	uint16_t filterval = (torqueFilterConf.params.freq & 0x1fff) | ((uint8_t)(torqueFilterConf.mode) << 13);
	Flash_Write(flashAddrs.torqueFilter, filterval);

}

/**
 * Writes ADC offsets into flash
 */
void TMC4671::saveAdcParams(){
	Flash_Write(flashAddrs.ADC_i0_ofs, conf.adc_I0_offset);
	Flash_Write(flashAddrs.ADC_i1_ofs, conf.adc_I1_offset);
	adcSettingsStored = true;
}

/**
 * Restores saved parameters
 * Call initialize() to apply some of the settings
 */
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
	uint16_t encofs;
	Flash_Read(flashAddrs.encOffset,&encofs);
	this->abnconf.posOffsetFromIndex = (int16_t)encofs;

	Flash_Read(flashAddrs.offsetFlux, (uint16_t*)&this->maxOffsetFlux);

	// Restore ADC settings
	if(Flash_Read(flashAddrs.ADC_i0_ofs,&conf.adc_I0_offset) &&	Flash_Read(flashAddrs.ADC_i1_ofs,&conf.adc_I1_offset)){
		adcSettingsStored = true; // Previous adc settings restored
	}else{
		recalibrationRequired = true; // Never stored
	}

//	abnconf.phiEoffset = Flash_ReadDefault(flashAddrs.phieOffset,0);
//	if(abnconf.phiEoffset != 0){
//		phiErestored = true;
//	}

	uint16_t miscval;
	if(Flash_Read(flashAddrs.encA, &miscval)){
		restoreEncHallMisc(miscval);
		encHallRestored = true;
	}
	uint16_t filterval;
	if(Flash_Read(flashAddrs.torqueFilter, &filterval)){
		torqueFilterConf.params.freq = filterval & 0x1fff;
		torqueFilterConf.mode = static_cast<TMCbiquadpreset>((filterval >> 13) & 0x7);
	}

}

bool TMC4671::hasPower(){
	int32_t intV = getIntV();
	return (intV > 10000) && (getExtV() > 10000) && (intV < 78000);
}

// Checks if important parameters are set to valid values
bool TMC4671::isSetUp(){

	if(this->conf.motconf.motor_type == MotorType::NONE ||!adcCalibrated || !initialized || !powerInitialized){
		return false;
	}

	// Encoder
	if(this->conf.motconf.phiEsource == PhiE::abn && abnconf.cpr == 0){
		return false;
	}
	if(this->conf.motconf.phiEsource == PhiE::abn || this->conf.motconf.phiEsource == PhiE::aenc){
		if(!encoderAligned){
			return false;
		}
	}
	if(this->conf.motconf.phiEsource == PhiE::ext && drvEncoder->getEncoderType() == EncoderType::NONE && !encoderAligned){
		return false;
	}

	return true;
}

/**
 * Check if driver is responding
 */
bool TMC4671::pingDriver(){
	writeReg(1, 0);
	return(readReg(0) == 0x34363731);
}

/**
 * Returns estimated VM in mV measured by TMC
 */
int32_t TMC4671::getTmcVM(){
	writeReg(0x03, 1); // adc raw data to VM/agpiA
	uint32_t agpiA_VM = readReg(0x02);
	agpiA_VM = (agpiA_VM & 0xFFFF) - 0x7FFF - conf.hwconf.adcOffset;

	return ((float)agpiA_VM * conf.hwconf.vmScaler) * 1000;
}


/**
 * Sets all parameters of the driver at startup. Only has to be called once when the driver is detected
 * restoreFlash() should be called before this to restore settings!
 */
bool TMC4671::initialize(){
//	active = true;
//	if(state == TMC_ControlState::uninitialized){
//		state = TMC_ControlState::Init_wait;
//	}
	// Check if a TMC4671 is active and replies correctly
	if(!pingDriver()){
		ErrorHandler::addError(communicationError);
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
		ES_TMCdetected = true;
	}

	if(!ES_TMCdetected){
		this->setPidPrecision(pidPrecision);
	}

	// Detect if tmc was previously uninitialized
	if(startupType == TMC_StartupType::NONE){
		if(getMotionMode() != MotionMode::stop){
			startupType = TMC_StartupType::warmStart;
		}else{
			startupType = TMC_StartupType::coldStart;
		}
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
	setTorqueFilter(torqueFilterConf);
	setBiquadFlux(TMC4671Biquad(Biquad(BiquadType::lowpass, fluxFilterFreq / getPwmFreq(), 0.7,0.0), true)); // Create flux filter

	// Initial adc calibration and check without PWM if power off to get basic offsets. PWM is off!
	if(!hasPower()){
		if(!calibrateAdcOffset(150)){
			changeState(TMC_ControlState::HardError); // ADC or shunt amp is broken!
			enablePin.reset();
			return false;
		}
	}
	// brake res failsafe.
//	/*
//	 * Single ended input raw value
//	 * 0V = 0x7fff
//	 * 4.7k / (360k+4.7k) Divider on old board.
//	 * 1.5k / (71.5k+1.5k) 16.121 counts 60V new. 100V VM => 2V
//	 * 13106 counts/V input.
//	 */
	setBrakeLimits(this->conf.hwconf.brakeLimLow,this->conf.hwconf.brakeLimHigh); // update limit from previously loaded constants or defaults

	// Status mask
	if(ES_TMCdetected){
		setStatusMask(0); // ES Version status output is broken
	}else{
		/*
		 * Enable adc clipping and pll errors
		 */
		statusMask.asInt = 0;
		statusMask.flags.adc_i_clipped = 1;
		statusMask.flags.not_PLL_locked = 1;
		setStatusMask(statusMask);
	}

	setPids(curPids); // Write basic pids

//	if(hasPower()){
//		enablePin.set();
//		setPwm(TMC_PwmMode::PWM_FOC);
//		calibrateAdcOffset(400); // Calibrate ADC again with power
//		motorEnabledRequested = true;
//	}
	//setEncoderType(conf.motconf.enctype);

	// Update flags
	readFlags(false); // Read all flags

	initialized = true;
	initTime = HAL_GetTick();
	return initialized;
}

/**
 * Reads a temperature from a thermistor connected to AGPI_B
 * Not calibrated perfectly!
 */
float TMC4671::getTemp(){
	if(!this->conf.hwconf.temperatureEnabled){
		return 0;
	}
	TMC4671HardwareTypeConf* hwconf = &conf.hwconf;

	writeReg(0x03, 2);
	int32_t adcval = ((readReg(0x02)) & 0xffff) - 0x7fff; // Center offset
	adcval -= hwconf->adcOffset;
	if(adcval <= 0){
		return 0.0;
	}
	float r = hwconf->thermistor_R2 * (((float)43252 / (float)adcval)); //43252 equivalent ADC count if it was 3.3V and not 2.5V

	// Beta
	r = (1.0 / 298.15) + log(r / hwconf->thermistor_R) / hwconf->thermistor_Beta;
	r = 1.0 / r;
	r -= 273.15;
	return r;

}

/**
 * Samples the adc and checks if it returns a neutral value
 */
bool TMC4671::checkAdc(){
	setFluxTorque(0, 0);
	int32_t total = 0;
	for(uint8_t i = 0;i<50;i++){
		std::pair<int32_t,int32_t> ft = getActualTorqueFlux();
		total += (ft.first+ft.second);
		Delay(2);
	}
	return(abs(total / 50) < 100); // Check if average has a low bias
}

void TMC4671::initializeWithPower(){
	if(powerInitialized){
		return;
	}
	powerInitialized = true;
	// Load ADC settings
	if(Flash_Read(flashAddrs.ADC_i0_ofs,&conf.adc_I0_offset) &&	Flash_Read(flashAddrs.ADC_i1_ofs,&conf.adc_I1_offset)){
		adcSettingsStored = true; // Previous adc settings restored
		setAdcOffset(conf.adc_I0_offset, conf.adc_I1_offset);
	}

	if(adcSettingsStored && checkAdc()){
		adcCalibrated = true;
	}else{
		if(!calibrateAdcOffset(300)){
			powerInitialized = false;
			return; // Abort
		}
	}

	// got power long enough. proceed to set up encoder
	// if encoder not set up
	enablePin.set();
	setPwm(TMC_PwmMode::PWM_FOC); // enable foc
	if(!encoderAligned){
		setEncoderType(conf.motconf.enctype);
	}else{
		//last state
		if(!emergency){
			allowStateChange = true;
			changeState(requestedState);
			setMotionMode(lastMotionMode,true);
			ErrorHandler::clearError(ErrorCode::undervoltage);
		}
	}

}

bool TMC4671::motorReady(){
	return this->state == TMC_ControlState::Running && powerInitialized && adcCalibrated && encoderAligned;
}

void TMC4671::Run(){
	// Main state machine
	while(1){

		if(!initialized && state != TMC_ControlState::HardError){
			changeState(TMC_ControlState::uninitialized,true);
		}

		// check if we are in a privileged state otherwise use requested state as new state
		if(allowStateChange){
			state = requestedState;
		}

		switch(this->state){

		case TMC_ControlState::uninitialized:
			allowStateChange = false;
			// check communication and write constants
			if(!pingDriver() || emergency){ // driver not available or emergency was set before startup
				initialized = false; // Assume driver is not initialized if we can not detect it
				Delay(250);
				break;
			}
			// Driver available. Write constants and go to next step
			if(!initialized){
				initialize();
			}
			changeState(TMC_ControlState::waitPower,true);
			break;

		case TMC_ControlState::waitPower:
		{
			allowStateChange = false;
			pulseClipLed(); // blink led
			static uint8_t powerCheckCounter = 0;
			// if powered check ADCs and go to encoder calibration
			if(!hasPower() || emergency){
				powerCheckCounter = 0;
				Delay(250);
				break;
			}
			if(++powerCheckCounter > 5 && !powerInitialized){
				initializeWithPower();
			}
			if(powerInitialized){
				allowStateChange = true;
			}
			Delay(100);
			break;
		}

		case TMC_ControlState::FullCalibration:
		{
			fullCalibrationInProgress = true;
			/*
			 * Wait for power (OK)
			 * Calibrate ADC offsets (OK)
			 * Measure motor response
			 * depending on encoder do encoder parameter estimation
			 * align and store phiE for single phase AENC or indexed ABN enc
			 *
			 * If at any point external movement is detected abort
			 */
			 // Wait for Power
			while(!hasPower()){
				Delay(100);
			}
			curFilters.flux.params.enable = false;
			setBiquadFlux(curFilters.flux);
			// Calibrate ADC
			enablePin.set();
			setPwm(TMC_PwmMode::PWM_FOC); // enable foc to calibrate adc
			Delay(50);
			if(calibrateAdcOffset(500)){
				saveAdcParams();
			}else{
				calibFailCb();
				break;
			}

			// Encoder
			calibrateEncoder();
			setEncoderType(conf.motconf.enctype);
			recalibrationRequired = false;
			curFilters.flux.params.enable = true;
			setBiquadFlux(curFilters.flux);
			break;
		}
		case TMC_ControlState::Pidautotune:
			{
				allowStateChange = false;
				 // Wait for Power
				while(!hasPower()){
					Delay(100);
				}
				pidAutoTune();
				allowStateChange = true;
				changeState(laststate,false);
				break;
			}

		case TMC_ControlState::IndexSearch:
			autohome();
			changeState(laststate);

			break;

		case TMC_ControlState::Running:
		{
			// Check status, Temps, Everything alright?
			uint32_t tick = HAL_GetTick();
			if(tick - lastStatTime > 2000){ // Every 2s
				lastStatTime = tick;
				statusCheck();
				// Get enable input. If tmc does not reply the result will read 0 or 0xffffffff (not possible normally)
				uint32_t pins = readReg(0x76);
				bool tmc_en = ((pins >> 15) & 0x01) && pins != 0xffffffff;
				if(!tmc_en && motorEnabledRequested){ // Hardware emergency.
					this->estopTriggered = true;
					this->emergencyStop(false);
					ErrorHandler::addError(estopError);
					//changeState(TMC_ControlState::HardError);
				}

				// Temperature sense
				if(conf.hwconf.temperatureEnabled){
					float temp = getTemp();
					if(temp > conf.hwconf.temp_limit){
						changeState(TMC_ControlState::OverTemp);
						pulseErrLed();
					}
				}

			}
			Delay(200);
		}
		break;

		case TMC_ControlState::Shutdown:
			Delay(100);
			if(estopTriggered){
				uint32_t pins = readReg(0x76);
				bool tmc_en = ((pins >> 15) & 0x01) && pins != 0xffffffff;
				if(tmc_en){
					// Emergency stop reset
					ErrorHandler::clearError(estopError);
					this->estopTriggered = false; // TODO resume correctly
					changeState(TMC_ControlState::uninitialized,true);
				}
			}
			break;

		case TMC_ControlState::EncoderInit:
			if(powerInitialized && hasPower())
				encoderInit();
		break;

		case TMC_ControlState::ExternalEncoderInit:
			if(powerInitialized && hasPower() && drvEncoder != nullptr)
				encoderInit();
			break;

		case TMC_ControlState::HardError:

		break; // Broken

		case TMC_ControlState::OverTemp:
			this->stopMotor();
			changeState(TMC_ControlState::HardError); // Block
		break;

		case TMC_ControlState::EncoderFinished: // Startup sequence done
			//setEncoderIndexFlagEnabled(false); // TODO
//			curFilters.flux.params.enable = true;
//			setBiquadFlux(curFilters.flux); // Enable flux filter
			encoderAligned = true;
			if(motorEnabledRequested && isSetUp()){
				startMotor();
				changeState(TMC_ControlState::Running);
			}else{
				stopMotor();
				laststate = TMC_ControlState::Running; // Go to running when starting again
			}

			if(fullCalibrationInProgress){
				fullCalibrationInProgress = false;
				Flash_Write(flashAddrs.encA,encodeEncHallMisc()); // Save encoder settings
			}


		break;

		default:

		break;
		}


		// Optional update methods for safety

		if(!hasPower() && state != TMC_ControlState::waitPower && initialized && powerInitialized){ // low voltage or overvoltage

			requestedState = state;
			ErrorHandler::addError(lowVoltageError);
			setMotionMode(MotionMode::stop,true); // Disable tmc
			changeState(TMC_ControlState::waitPower,true);
			allowStateChange = false;
		}

		if(flagCheckInProgress){ // cause some delay until reenabling the status interrupt checking
			setStatusFlags(0);
			flagCheckInProgress = false;
		}
		Delay(10);

		if(emergency && !motorReady()){
			this->Suspend(); // we can not safely run. wait until resumed by estop
		}
	} // End while
}

void TMC4671::calibrateEncoder(){
	if(conf.motconf.enctype == EncoderType_TMC::abn) {
		estimateABNparams();
		// Report changes
		CommandHandler::broadcastCommandReply(CommandReply(abnconf.npol ? 1 : 0), (uint32_t)TMC4671_commands::encpol, CMDtype::get);
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		calibrateAenc();
	}else if(conf.motconf.enctype == EncoderType_TMC::ext){
		estimateExtEnc();
	}
	changeState(TMC_ControlState::EncoderInit);


}

/**
 * Iterative tuning function for tuning the torque mode PI values
 */
bool TMC4671::pidAutoTune(){
	/**
	 * Enter phieExt & torque mode
	 * Zero I, default P
	 * Ramp up flux P until 50% of target, then lower increments until targetflux_p is reached
	 * Increase I until oscillation is found. Back off a bit
	 */
	curFilters.flux.params.enable = false;
	setBiquadFlux(curFilters.flux);
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setPhiE_ext(getPhiE());
	setPhiEtype(PhiE::ext); // Fixed phase
	setMotionMode(MotionMode::torque, true);
	setFluxTorque(0, 0);
	TMC4671PIDConf newpids = curPids;
	int16_t targetflux = std::min<int16_t>(this->curLimits.pid_torque_flux,bangInitPower); // Respect limits
	int16_t targetflux_p = targetflux * 0.75;

	uint16_t fluxI = 0,fluxP = 100; // Startvalues
	writeReg(0x54, fluxI | (fluxP << 16));
	int32_t flux = 0;
	setFluxTorque(targetflux, 0); // Start flux step
	while(fluxP < 20000){
		writeReg(0x54, fluxI | (fluxP << 16)); // Update P
		Delay(50); // Wait a bit. not critical
		flux = getActualFlux();
		if(flux > targetflux_p){
			break;
		}else if(flux > targetflux * 0.5){
			// Reduce steps when we are close
			fluxP+=10;
		}else{
			fluxP+=100;
		}
	}
	setFluxTorque(0, 0);
	Delay(100); // Let the current settle down

	// Tune I. This is more difficult because we need to take overshoot into account
	uint32_t measuretime = 50; // ms to wait per measurement
	uint16_t step_i = 64;
	fluxI = 100;
	flux = 0;
	while(fluxI < 20000){
		writeReg(0x54, fluxI | (fluxP << 16));
		uint32_t tick = HAL_GetTick();//micros();
		int32_t peakflux = 0;
		setFluxTorque(targetflux, 0);

		while(HAL_GetTick() - tick < measuretime){ // Measure current for this pulse
			flux = getActualFlux();
			peakflux = std::max<int32_t>(peakflux, flux);
		}
		setFluxTorque(0, 0);
		uint8_t timeout = 100;  // Let the current settle down
		while(timeout-- && flux > 10){
			Delay(1);
			flux = getActualFlux();
		}

		if(peakflux > (targetflux + ( targetflux * 0.03))) // Overshoot target by 3%
		{
			fluxI -= step_i; // Revert last step
			break;
		}
		if(peakflux < targetflux*0.95){ // Do larger steps if we don't even reach near the target within the time.
			step_i = 100;
		}else{
			step_i = 10;
		}
		fluxI += step_i;

	}
	curFilters.flux.params.enable = true;
	setBiquadFlux(curFilters.flux);

	if(fluxP && fluxP < 20000 && fluxI && fluxI < 20000){
			newpids.fluxP = fluxP;
			newpids.torqueP = fluxP;
			newpids.fluxI = fluxI;
			newpids.torqueI = fluxI;
	}else{
		CommandHandler::broadcastCommandReply(CommandReply("PID Autotune failed",0), (uint32_t)TMC4671_commands::pidautotune, CMDtype::get);
		setPhiEtype(lastphie);
		setMotionMode(lastmode,true);
		return false;
	}

	setPids(newpids); // Apply new values
	CommandHandler::broadcastCommandReply(CommandReply("PID Autotune success",1), (uint32_t)TMC4671_commands::pidautotune, CMDtype::get);
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);
	return true;
}

bool TMC4671::autohome(){
	// Moves motor to index
	if(findEncoderIndex(abnconf.posOffsetFromIndex < 0 ? 10 : -10,bangInitPower/2,false,false)){
		// Load position offset
		if(abnconf.useIndex)
			setTmcPos(getPosAbs() - abnconf.posOffsetFromIndex);
		return true;
	}
	return false;
}

/*
 * Returns the current state of the driver controller
 */
TMC_ControlState TMC4671::getState(){
	return this->state;
}

inline void TMC4671::changeState(TMC_ControlState newState,bool force){
	if(newState != this->state){
		this->laststate = this->state; // save last state if new state wants to jump back
	}
	if(!force){
		this->requestedState = newState;
	}else{
		state = newState;
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

void TMC4671::zeroAbnUsingPhiM(bool offsetPhiE){
	int32_t npos = (int32_t)readReg(0x28); // raw encoder counts at index hit
	int32_t npos_M = (npos * 0xffff) / abnconf.cpr; // Scaled encoder angle at index
	abnconf.phiMoffset = -npos_M;
	if(offsetPhiE){
		abnconf.phiEoffset += npos_M*conf.motconf.pole_pairs;
		// change index to zero phiM
		uint32_t phiEphiM = readReg(0x29);
		int16_t phiE = ((phiEphiM >> 16) & 0xffff); // Write back phiE offset
		int16_t phiM = phiEphiM & 0xffff;
		//updateReg(0x29, abnconf.phiMoffset, 0xffff, 0);
		writeReg(0x29,(phiE << 16) | phiM);
	}else{
		updateReg(0x29, abnconf.phiMoffset, 0xffff, 0);
	}
	setTmcPos(getPosAbs()); // Set position to absolute position = ~zero
}

/**
 * Rotates motor until the ABN index is found
 */
bool TMC4671::findEncoderIndex(int32_t speed, uint16_t power,bool offsetPhiM,bool zeroCount){

	if(conf.motconf.enctype != EncoderType_TMC::abn){
		return false; // Only valid for ABN encoders
	}

	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	curFilters.flux.params.enable = false;
	setBiquadFlux(curFilters.flux);
	setFluxTorque(0, 0);
//	setPhiE_ext(getPhiE());
//	setPhiEtype(PhiE::openloop);

//	abnconf.clear_on_N = true;
//	setup_ABN_Enc(abnconf);

	// Arm encoder signal
	setEncoderIndexFlagEnabled(true,zeroCount);
	// Rotate

	//uint32_t mposStart = readReg(0x2A);
	int32_t timeout = 1000; // 10s
	rampFlux(power, 500);
	runOpenLoop(power, 0, speed, 10, true);
	while(!encoderIndexHitFlag && timeout-- > 0){
		Delay(10);
	}
	//int32_t speed = 10;
	rampFlux(0, 100);
	runOpenLoop(0, 0, 0, 10, true);
	if(!encoderIndexHitFlag){
		pulseErrLed();
		ErrorHandler::addError(indexNotHitError);
	}

	// If zero count on index write a phiM offset so that phiM is 0 on index and we don't need to change the raw encoder count (possible timing danger)
	if(offsetPhiM){
		zeroAbnUsingPhiM(false);
	}

//	abnconf.clear_on_N = false;
//	setup_ABN_Enc(abnconf);
	curFilters.flux.params.enable = true;
	setBiquadFlux(curFilters.flux);

	setMotionMode(lastmode,true);
	setPhiEtype(lastphie);
	return encoderIndexHitFlag;
}

/**
 * Enables or disables the encoder index interruption on the flag pin depending on the selected encoder
 */
void TMC4671::setEncoderIndexFlagEnabled(bool enabled,bool zeroEncoder){
	//zeroEncoderOnIndexHit = zeroEncoder;

	updateReg(0x25, zeroEncoder ? 1 : 0, 0x1, 9); // Enable encoder clearing
	if(zeroEncoder){
		writeReg(0x28,0); // Preload 0 into n register
	}

	if(enabled)
		encoderIndexHitFlag = false;
	setStatusFlags(0); // Reset flags
	this->statusMask.flags.AENC_N = this->conf.motconf.enctype == EncoderType_TMC::sincos && enabled;
	this->statusMask.flags.ENC_N = this->conf.motconf.enctype == EncoderType_TMC::abn && enabled;
	setStatusMask(statusMask); // Enable flag output for encoder
}

/**
 * Enables position mode and sets a target position
 */
void TMC4671::setTargetPos(int32_t pos){
	if(curMotionMode != MotionMode::position){
		setMotionMode(MotionMode::position,true);
		setPhiEtype(this->conf.motconf.phiEsource);
	}
	writeReg(0x68,pos);
}
int32_t TMC4671::getTargetPos(){

	return readReg(0x68);
}


/**
 * Enables velocity mode and sets a velocity target
 */
void TMC4671::setTargetVelocity(int32_t vel){
	if(curMotionMode != MotionMode::velocity){
		setMotionMode(MotionMode::velocity,true);
		setPhiEtype(this->conf.motconf.phiEsource);
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

int16_t TMC4671::getPhiEfromExternalEncoder(){
	int64_t phiE_t = (int64_t)drvEncoder->getPosAbs() * 0xffff;
	if(this->conf.encoderReversed){
		phiE_t = -phiE_t;
	}
	int32_t phiE = (phiE_t / (int64_t)drvEncoder->getCpr());
	phiE = (phiE * conf.motconf.pole_pairs) & 0xffff; // scale to pole pairs
	//int16_t phiE = (drvEncoder->getPosAbs_f() * (float)0xffff) * conf.motconf.pole_pairs + externalEncoderPhieOffset;
	return(phiE+externalEncoderPhieOffset);
}

// PhiE is read only
int16_t TMC4671::getPhiE(){
	return readReg(0x53);
}



/**
 * Aligns ABN encoders by forcing an angle with high current and calculating the offset
 * Will start at the current phiE to minimize any extra movements (useful if motor was turned in openloop mode before already)
 * @param power Maximum current reached during flux ramp
 */
void TMC4671::bangInitEnc(int16_t power){
	if(!hasPower() || (this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC)){ // If not stepper or bldc return
		return;
	}
	blinkClipLed(50, 0);
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setFluxTorque(0, 0);

	uint8_t phiEoffsetReg = 0;
	if(conf.motconf.enctype == EncoderType_TMC::abn){
		phiEoffsetReg = 0x29;
		if(!encoderIndexHitFlag)
			zeroAbnUsingPhiM();
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		writeReg(0x41,0); //Zero encoder
		writeReg(0x47,0); //Zero encoder
		phiEoffsetReg = 0x45;
	}else if (usingExternalEncoder()){
		externalEncoderPhieOffset = 0;
	}else{
		return; // Not relevant
	}

	//setTmcPos(0);

	//setMotionMode(MotionMode::uqudext);

	//Delay(100);
	int16_t phiEpos = getPhiE();// readReg(phiEreg)>>16; // starts at current encoder position
	updateReg(phiEoffsetReg, 0, 0xffff, 16); // Set phiE offset to zero
	setPhiE_ext(phiEpos);
	setPhiEtype(PhiE::ext);
	// Ramp up flux
	rampFlux(power, 1000);
	int16_t phiE_enc = getPhiE_Enc();

	Delay(50);
	int16_t phiE_abn_old = 0;
	int16_t c = 0;
	uint16_t still = 0;
	while(still < 30 && c++ < 1000){
		// Wait for motor to stop moving
		if(abs(phiE_enc - phiE_abn_old) < 100){
			still++;
		}else{
			still = 0;
		}
		phiE_abn_old = phiE_enc;

		phiE_enc = getPhiE_Enc();

		//phiE_enc=readReg(phiEreg)>>16;
		Delay(10);
	}
	rampFlux(0, 100);

	//Write offset
	//int16_t phiE_abn = readReg(0x2A)>>16;
	int16_t phiEoffset =  phiEpos-phiE_enc;

	if(phiEoffset == 0){ // 0 invalid
		phiEoffset = 1;
	}
	if (usingExternalEncoder()){
		externalEncoderPhieOffset = phiEoffset;
	}else{
		updateReg(phiEoffsetReg, phiEoffset, 0xffff, 16);
	}

	if(conf.motconf.enctype == EncoderType_TMC::abn){
		abnconf.phiEoffset = phiEoffset;
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		aencconf.phiEoffset = phiEoffset;
	}


	setPhiE_ext(0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);
	//setTmcPos(pos+getPos());
	//setTmcPos(0);

	blinkClipLed(0, 0);
}

/**
 * Moves the motor to find out analog encoder scalings and offsets
 */
void TMC4671::calibrateAenc(){

	// Rotate and measure min/max
	blinkClipLed(250, 0);
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	//int32_t pos = getPos();
	PosSelection possel = this->conf.motconf.pos_sel;
	setPosSel(PosSelection::PhiE_openloop);
	setTmcPos(0);
	// Ramp up flux
	setFluxTorque(0, 0);
	writeReg(0x23,0); // set phie openloop 0
	setPhiEtype(PhiE::openloop);
	setMotionMode(MotionMode::torque,true);

	if(this->conf.motconf.motor_type == MotorType::STEPPER || this->conf.motconf.motor_type == MotorType::BLDC){
		rampFlux(bangInitPower, 250);
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
			rampFlux(0, 100);
			runOpenLoop(0, 0, 0, 1000,true);
			Delay(250);
			// Zero aenc
			writeReg(0x41, 0);
			initialDirPos = readReg(0x41);
			runOpenLoop(bangInitPower, 0, speed, 100,true);
			stage = 2;
		}else if(getPos() > 0 && stage == 2){
			stage = 3;
			rampFlux(0, 100);
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
	setTmcPos(0);

	blinkClipLed(0, 0);
}

/**
 * Reads phiE directly from the encoder selection instead of the current phiE selection
 */
int16_t TMC4671::getPhiE_Enc(){
	if(conf.motconf.enctype == EncoderType_TMC::abn){
		return (int16_t)(readReg(0x2A)>>16);
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		return (int16_t)(readReg(0x46)>>16);
	}else if(conf.motconf.enctype == EncoderType_TMC::hall){
		return (int16_t)(readReg(0x39)>>16);
	}else if(usingExternalEncoder()){
		return getPhiEfromExternalEncoder();
	}else{
		return getPhiE();
	}
}

/**
 * Steps the motor a few times to check if the encoder follows correctly
 */
bool TMC4671::checkEncoder(){
	if(this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC &&
			conf.motconf.enctype != EncoderType_TMC::uvw && conf.motconf.enctype != EncoderType_TMC::sincos && conf.motconf.enctype != EncoderType_TMC::abn && conf.motconf.enctype != EncoderType_TMC::ext)
	{ // If not stepper or bldc return
		return true;
	}
	blinkClipLed(150, 0);

	const uint16_t maxcount = 50; // Allowed reversals
	const uint16_t maxfail = 10; // Allowed fails
	const int16_t startAngle = getPhiE_Enc(); // Start angle offsets all angles later so there is no jump if angle is already properly aligned
	const int16_t targetAngle = 0x3FFF;

	bool result = true;
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setFluxTorque(0, 0);
	setPhiEtype(PhiE::ext);

	setPhiE_ext(startAngle);
	// Ramp up flux
	rampFlux(2*bangInitPower/3, 250);

	//Forward
	int16_t phiE_enc = 0;
	uint16_t failcount = 0;
	int16_t revCount = 0;
	for(int16_t angle = 0;angle<targetAngle;angle+=0x00ff){
		uint16_t c = 0;
		setPhiE_ext(angle+startAngle);
		Delay(5);
		phiE_enc = getPhiE_Enc() - startAngle;
		int16_t err = abs(phiE_enc - angle);
		int16_t nErr = abs(phiE_enc + angle);
		// Wait more until encoder settles a bit
		while(err > 2000 && nErr > 2000 && c++ < 50){
			phiE_enc = getPhiE_Enc() - startAngle;
			err = abs(phiE_enc - angle);
			nErr = abs(angle - phiE_enc);
			Delay(10);
		}
		if(err > nErr){
			revCount++;
		}
		if(c >= maxcount){
			failcount++;
			if(failcount > maxfail){
				result = false;
				break;
			}
		}
	}
	/* If we are still at the start angle the encoder did not move at all.
	 * Possible issues:
	 * Encoder connection wrong
	 * Wrong encoder selection
	 * No motor movement
	 * No encoder power
	 */
	if(startAngle == getPhiE_Enc()){
		ErrorHandler::addError(Error(ErrorCode::encoderAlignmentFailed,ErrorType::critical,"Encoder did not move during alignment"));
		this->changeState(TMC_ControlState::HardError, true);
		result = false;
	}

	// Backward

	if(result){ // Only if not already failed
		for(int16_t angle = targetAngle;angle>0;angle -= 0x00ff){
			uint16_t c = 0;
			setPhiE_ext(angle+startAngle);
			Delay(5);
			phiE_enc = getPhiE_Enc() - startAngle;
			int16_t err = abs(phiE_enc - angle);
			int16_t nErr = abs(phiE_enc + angle);
			// Wait more
			while(err > 2500 && nErr > 2500 && c++ < 50){
				phiE_enc = getPhiE_Enc() - startAngle;
				err = abs(phiE_enc - angle);
				nErr = abs(angle - phiE_enc);
				Delay(10);
			}
			if(err > nErr){
				revCount++;
			}
			if(c >= maxcount){
				failcount++;
				if(failcount > maxfail){
					result = false;
					break;
				}
			}
		}
	}

	// TODO check if we want that
	if(revCount > maxcount){ // Encoder seems reversed
		// reverse encoder
		if(this->conf.motconf.enctype == EncoderType_TMC::abn){
			this->abnconf.rdir = !this->abnconf.rdir;
			setup_ABN_Enc(abnconf);
		}else if(this->conf.motconf.enctype == EncoderType_TMC::sincos || this->conf.motconf.enctype == EncoderType_TMC::uvw){
			this->aencconf.rdir = !this->aencconf.rdir;
			setup_AENC(aencconf);
		}else if(this->conf.motconf.enctype == EncoderType_TMC::ext){
			this->conf.encoderReversed = !this->conf.encoderReversed;
		}
		ErrorHandler::addError(Error(ErrorCode::encoderReversed,ErrorType::warning,"Encoder direction reversed during check"));
	}

	rampFlux(0, 100);
	setPhiE_ext(0);
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);

	if(result){
		encoderAligned = true;
	}
	blinkClipLed(0, 0);
	return result;
}

void TMC4671::setup_ABN_Enc(TMC4671ABNConf encconf){
	this->abnconf = encconf;
	this->conf.encoderReversed = encconf.rdir;
	uint32_t abnmode =
			(encconf.apol |
			(encconf.bpol << 1) |
			(encconf.npol << 2) |
			(encconf.ab_as_n << 3) |
			(encconf.latch_on_N << 8) |
			(encconf.rdir << 12));

	writeReg(0x25, abnmode);
	//int32_t pos = getPos();
	writeReg(0x26, encconf.cpr);
	writeReg(0x29, ((uint16_t)encconf.phiEoffset << 16) | (uint16_t)encconf.phiMoffset);
	//setTmcPos(pos);
	//writeReg(0x27,0); //Zero encoder
	//conf.motconf.phiEsource = PhiE::abn;
	if(encconf.useIndex){
		encoderIndexHitFlag = false; // Reset flag
	}


}
void TMC4671::setup_AENC(TMC4671AENCConf encconf){
	this->conf.encoderReversed = encconf.rdir;
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


/**
 * Calibrates the ADC by disabling the power stage and sampling a mean value. Takes time!
 */
bool TMC4671::calibrateAdcOffset(uint16_t time){

	uint16_t measuretime_idle = time;
	uint32_t measurements_idle = 0;
	uint64_t totalA=0;
	uint64_t totalB=0;
	bool allowTemp = conf.hwconf.temperatureEnabled;
	conf.hwconf.temperatureEnabled = false; // Temp check interrupts adc
	writeReg(0x03, 0); // Read raw adc
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	setMotionMode(MotionMode::stop,true);
	Delay(100); // Wait a bit before sampling
	uint16_t lastrawA=conf.adc_I0_offset, lastrawB=conf.adc_I1_offset;

	//pulseClipLed(); // Turn on led
	// Disable drivers and measure many samples of zero current
	//enablePin.reset();
	uint32_t tick = HAL_GetTick();
	while(HAL_GetTick() - tick < measuretime_idle){ // Measure idle
		writeReg(0x03, 0); // Read raw adc
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
//		uint32_t lastMicros = micros();
//		while(micros()-lastMicros < 100){} // Wait 100µs at least
	}
	//enablePin.set();
	int32_t offsetAidle = totalA / (measurements_idle);
	int32_t offsetBidle = totalB / (measurements_idle);

	// Check if offsets are in a valid range
	if(totalA < 100 || totalB < 100 || ((abs(offsetAidle - 0x7fff) > TMC_ADCOFFSETFAIL) || (abs(offsetBidle - 0x7fff) > TMC_ADCOFFSETFAIL)) ){
		ErrorHandler::addError(Error(ErrorCode::adcCalibrationError,ErrorType::critical,"TMC ADC offset calibration failed."));
//		blinkErrLed(100, 0); // Blink forever
//		setPwm(TMC_PwmMode::off); //Disable pwm
//		this->changeState(TMC_ControlState::HardError);
		adcCalibrated = false;
		conf.hwconf.temperatureEnabled = allowTemp;
		return false; // An adc or shunt amp is likely broken. do not proceed.
	}
	conf.adc_I0_offset = offsetAidle;
	conf.adc_I1_offset = offsetBidle;
	setAdcOffset(conf.adc_I0_offset, conf.adc_I1_offset);
	// ADC Offsets should now be close to perfect

	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);
	adcCalibrated = true;
	conf.hwconf.temperatureEnabled = allowTemp;
	return true;
}

void TMC4671::calibFailCb(){
	if(calibrationFailCount-- != 0){
		changeState(TMC_ControlState::FullCalibration); // retry
	}else{
		Error err = Error(ErrorCode::tmcCalibFail,ErrorType::critical,"TMC calibration failed");
		ErrorHandler::addError(err);
		changeState(TMC_ControlState::HardError);
	}
}
void TMC4671::encoderInit(){

	if(!powerInitialized || !hasPower()){
		changeState(TMC_ControlState::waitPower);
		requestedState = TMC_ControlState::EncoderInit;
		return;
	}

	// Initializes encoder
	if(conf.motconf.enctype == EncoderType_TMC::abn){
		setPosSel(PosSelection::PhiM_abn); // Mechanical Angle
		setVelSel(VelSelection::PhiM_abn); // Mechanical Angle (RPM)
		//setup_ABN_Enc(abnconf);
		if(!encHallRestored){
			estimateABNparams(); // If not saved try to estimate parameters
			recalibrationRequired = true;
		}
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		setPosSel(PosSelection::PhiM_aenc); // Mechanical Angle
		setVelSel(VelSelection::PhiM_aenc); // Mechanical Angle (RPM)
		//setup_AENC(aencconf);
		calibrateAenc();
	}

	// find index

	if(conf.motconf.enctype == EncoderType_TMC::abn && abnconf.useIndex && !encoderIndexHitFlag){ // TODO changing direction might invalidate phiE offset because of index pulse width
		findEncoderIndex(abnconf.posOffsetFromIndex < 0 ? 10 : -10,bangInitPower/2,true,true); // Go to index and zero encoder
		setPhiEtype(PhiE::openloop); // Openloop used in last step. Use for aligning too
	}else{
		setPhiE_ext(getPhiE());
		setPhiEtype(PhiE::ext);
	}

	// Align encoder
	// TODO handle absolute external encoders
	bangInitEnc(bangInitPower);

	// Check encoder
	if(!checkEncoder()){
		if(++enc_retry > enc_retry_max){
			encoderAligned = false;
			Error err = Error(ErrorCode::encoderAlignmentFailed,ErrorType::critical,"Encoder alignment failed");
			ErrorHandler::addError(err);
			stopMotor();
			allowStateChange = false;
			changeState(TMC_ControlState::HardError,true);
		}

		if(manualEncAlign){
			manualEncAlign = false;
			CommandHandler::broadcastCommandReply(CommandReply("Error during check",1), (uint32_t)TMC4671_commands::encalign, CMDtype::get);
		}
		return;
	}
	encoderAligned = true;



	if(conf.motconf.enctype == EncoderType_TMC::abn && abnconf.useIndex && encoderIndexHitFlag)
		setTmcPos(getPosAbs() - abnconf.posOffsetFromIndex); // Load stored position

	if(manualEncAlign){
		manualEncAlign = false;
		CommandHandler::broadcastCommandReply(CommandReply("Aligned successfully",1), (uint32_t)TMC4671_commands::encalign, CMDtype::get);
	}
	changeState(TMC_ControlState::EncoderFinished);

	if(conf.motconf.enctype == EncoderType_TMC::abn){
		setPhiEtype(PhiE::abn);
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		setPhiEtype(PhiE::aenc);
	}else if(usingExternalEncoder()){
//		setPosSel(PosSelection::PhiE_ext);
//		setVelSel(VelSelection::PhiE_ext); // Mechanical Angle (RPM)
		setPhiEtype(PhiE::extEncoder);
	}

}

/**
 * Changes the encoder type and calls init methods for the encoder types.
 * Setup the specific parameters (abnconf, aencconf...) first.
 */
void TMC4671::setEncoderType(EncoderType_TMC type){
	// If no external timer is set external encoder is not valid
	if((!externalEncoderTimer || !externalEncoderAllowed()) && type == EncoderType_TMC::ext){
		type = EncoderType_TMC::NONE;
	}
	this->conf.motconf.enctype = type;
	this->statusMask.flags.AENC_N = 0;
	this->statusMask.flags.ENC_N = 0;
	//encoderIndexHitFlag = false;
	setStatusMask(statusMask);
	encoderAligned = false;

	abnconf.rdir = this->conf.encoderReversed;
	aencconf.rdir = this->conf.encoderReversed;

	if(type == EncoderType_TMC::abn){
		encoderAligned = false;
		// Not initialized if cpr not set
		if(this->abnconf.cpr == 0){
			return;
		}
		changeState(TMC_ControlState::EncoderInit);

		setup_ABN_Enc(abnconf);

	// SinCos encoder
	}else if(type == EncoderType_TMC::sincos){
		encoderAligned = false;
		changeState(TMC_ControlState::EncoderInit);
		this->aencconf.uvwmode = false; // sincos mode
		setup_AENC(aencconf);

	// Analog UVW encoder
	}else if(type == EncoderType_TMC::uvw){
		encoderAligned = false;
		changeState(TMC_ControlState::EncoderInit);
		this->aencconf.uvwmode = true; // uvw mode
		setup_AENC(aencconf);

	}else if(type == EncoderType_TMC::hall){ // Hall sensor. Just trust it
		changeState(TMC_ControlState::Shutdown);
		setPosSel(PosSelection::PhiM_hal);
		setVelSel(VelSelection::PhiM_hal);
		encoderAligned = true;
		setPhiEtype(PhiE::hall);
		setup_HALL(hallconf);

	}else if(type == EncoderType_TMC::ext && drvEncoder && drvEncoder->getEncoderType() != EncoderType::NONE){
		// TODO check different encoder type
		encoderAligned = false;
		setUpExtEncTimer();
		//changeState(TMC_ControlState::Shutdown);
		changeState(TMC_ControlState::ExternalEncoderInit);
	}else{
		changeState(TMC_ControlState::Shutdown);
		encoderAligned = true;
	}

}

uint32_t TMC4671::getEncCpr(){
	EncoderType_TMC type = conf.motconf.enctype;
	if(type == EncoderType_TMC::abn || type == EncoderType_TMC::NONE){
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

	// External encoder is phiE ext but enables constant phiE updates too
	if(type == PhiE::extEncoder){
		type = PhiE::ext;
	}

	writeReg(0x52, (uint8_t)type & 0xff);
}
PhiE TMC4671::getPhiEtype(){
	PhiE phie = PhiE(readReg(0x52) & 0x7);
	if(phie == PhiE::ext && conf.motconf.phiEsource == PhiE::extEncoder){
		return PhiE::extEncoder;
	}
	return phie;
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
	startMotor();
	if(torqueMode){
		if(this->conf.motconf.motor_type == MotorType::DC){
			uq = ud+uq; // dc motor has no flux. add to torque
		}
		setFluxTorque(ud, uq);
	}else{
		setMotionMode(MotionMode::uqudext,true);
		setUdUq(ud,uq);
	}
	int16_t oldPhiE = getPhiE();
	setPhiEtype(PhiE::openloop);
	writeReg(0x23,oldPhiE); // Start running at last phiE value

	setOpenLoopSpeedAccel(speed, accel);
}

void TMC4671::setUdUq(int16_t ud,int16_t uq){
	writeReg(0x24, ud | (uq << 16));
}

void TMC4671::stopMotor(){
	// Stop driver if running

//	enablePin.reset();
	motorEnabledRequested = false;
	if(state == TMC_ControlState::Running || state == TMC_ControlState::EncoderFinished){
		setMotionMode(MotionMode::stop,true);
		setPwm(TMC_PwmMode::off); // disable foc
		changeState(TMC_ControlState::Shutdown);
	}
}
void TMC4671::startMotor(){
	motorEnabledRequested = true;

	if(state == TMC_ControlState::Shutdown && initialized && encoderAligned){
		changeState(TMC_ControlState::Running);
	}
	// Start driver if powered and emergency flag reset
	if(hasPower() && !emergency){
		setPwm(TMC_PwmMode::PWM_FOC); // enable foc
		enablePin.set();
		setMotionMode(nextMotionMode,true);

	}
	else{
		changeState(TMC_ControlState::waitPower);
	}

}

void TMC4671::emergencyStop(bool reset){
	if(!reset){
//		setPwm(TMC_PwmMode::HSlow_LShigh); // Short low side for instant stop
		emergency = true;
		enablePin.reset(); // Release enable pin to disable the whole driver
		motorEnabledRequested = false;
		this->stopMotor();

	}else{
//		enablePin.set();
//		writeReg(0x64, 0); // Set flux and torque 0 directly. Make sure motor does not jump
//		setPwm(TMC_PwmMode::PWM_FOC);
		emergency = false;
		motorEnabledRequested = true;
		//this->changeState(TMC_ControlState::waitPower, true); // Reinit
		this->startMotor();
		if(!motorReady()){
			if(inIsr()){
				ResumeFromISR();
			}else{
				Resume();
			}
		}
	}
}

/**
 * Calculates a flux value based on the internal and external voltage difference to dissipate energy without
 * a brake resistor
 */
int16_t TMC4671::controlFluxDissipate(){

	int32_t vDiff = getIntV() - getExtV();
	if(vDiff > fluxDissipationLimit){
		// Reaches limit at +5v if scaler is 1
		return(clip<int32_t,int32_t>(vDiff * conf.hwconf.fluxDissipationScaler * curLimits.pid_torque_flux * 0.0002,0,curLimits.pid_torque_flux));
	}
	return 0;
}

/**
 * Sets a torque in positive or negative direction
 * For ADC linearity reasons under 25000 is recommended
 */
void TMC4671::turn(int16_t power){
	if(!(this->motorReady() && motorEnabledRequested))
		return;
	int32_t flux = 0;

	// Flux offset for field weakening

	flux = idleFlux-clip<int32_t,int16_t>(abs(power),0,maxOffsetFlux);
	if((this->conf.encoderReversed && conf.motconf.enctype == EncoderType_TMC::ext) ^ conf.invertForce){
		power = -power; // Encoder does not match
	}

	/*
	 * If flux dissipation is on prefer this over the resistor.
	 * Warning: The axis only calls this function when active and if torque changed.
	 * It may not update during sustained force and still cause overvoltage conditions.
	 * TODO periodically check and update if driver is on but no torque update is sent
	 */
	if(conf.hwconf.fluxDissipationScaler && conf.enableFluxDissipation){
		int16_t dissipationFlux = controlFluxDissipate();
		if(dissipationFlux != 0){
			flux = dissipationFlux;
		}
	}

	setFluxTorque(flux, power);
}

/**
 * Changes the position sensor source
 */
void TMC4671::setPosSel(PosSelection psel){
	writeReg(0x51, (uint8_t)psel);
	this->conf.motconf.pos_sel = psel;
}

/**
 * Changes the velocity sensor source (RPM if PhiM source used)
 * @param mode 0 = fixed frequency sampling (~4369.067Hz), 1 = PWM sync time difference measurement
 */
void TMC4671::setVelSel(VelSelection vsel,uint8_t mode){
	uint32_t vselMode = ((uint8_t)vsel & 0xff) | ((mode & 0xff) << 8);
	writeReg(0x50, vselMode);
	this->conf.motconf.vel_sel = vsel;
}

/**
 * Changes the mode of the 8 GPIO pins
 * Banks A and B can be mapped independently to inputs or outputs, as a DS adc interface or by default as a secondary debug SPI port
 */
void TMC4671::setGpioMode(TMC_GpioMode mode){
	uint8_t modeReg = 0;
	switch(mode){
	default:
	case TMC_GpioMode::DebugSpi:
		modeReg = 0;			break;
	case TMC_GpioMode::DSAdcClkOut:
		modeReg = 0b1100111;	break;
	case TMC_GpioMode::DSAdcClkIn:
		modeReg = 0b0000111;	break;
	case TMC_GpioMode::Ain_Bin:
		modeReg = 0b0000001;	break;
	case TMC_GpioMode::Ain_Bout:
		modeReg = 0b0010001;	break;
	case TMC_GpioMode::Aout_Bin:
		modeReg = 0b0001001;	break;
	case TMC_GpioMode::Aout_Bout:
		modeReg = 0b0011001;	break;
	}

	updateReg(0x7B, modeReg,0xff,0);
}

/**
 * Reads the state of the 8 gpio pins
 */
uint8_t TMC4671::getGpioPins(){
	return readReg(0x7B) >> 24;
}

/**
 * Changes the state of gpio pins that are mapped as output
 * lower 4 bits bank A, upper 4 bits bank B
 */
void TMC4671::setGpioPins(uint8_t pins){
	uint32_t reg = pins << 16;
	updateReg(0x7B, reg,0xff,16);
}


/**
 * Returns a string with the name and version of the chip
 */
std::pair<uint32_t,std::string> TMC4671::getTmcType(){

	std::string reply = "";
	writeReg(1, 0);
	uint32_t nameInt = readReg(0);
	if(nameInt == 0 || nameInt ==  0xffffffff){
		reply = "No driver connected";
		return std::pair<uint32_t,std::string>(0,reply);
	}

	nameInt = __REV(nameInt);
	char* name = reinterpret_cast<char*>(&nameInt);
	std::string namestring = std::string(name,sizeof(nameInt));

	writeReg(1, 1);
	uint32_t versionInt = readReg(0);

	std::string versionstring = std::to_string((versionInt >> 16) && 0xffff) + "." + std::to_string((versionInt) && 0xffff);

	reply += "TMC" + namestring + " v" + versionstring;
	return std::pair<uint32_t,std::string>(versionInt,reply);
}

Encoder* TMC4671::getEncoder(){
	if((conf.motconf.enctype == EncoderType_TMC::ext && externalEncoderTimer) || conf.combineEncoder){
		return MotorDriver::drvEncoder.get();
	}else{
		return static_cast<Encoder*>(this);
	}
}

void TMC4671::setEncoder(std::shared_ptr<Encoder>& encoder){
	MotorDriver::drvEncoder = encoder;
	if(conf.motconf.enctype == EncoderType_TMC::ext && externalEncoderTimer){
		// TODO Calibrate and align external encoder
		changeState(TMC_ControlState::ExternalEncoderInit);
	}
}

bool TMC4671::hasIntegratedEncoder(){
	// Use internal encoder if not external encoder is selected
	return conf.motconf.enctype != EncoderType_TMC::ext && !this->conf.combineEncoder;
}

/**
 * Changes position using offset from index
 */
void TMC4671::setPos(int32_t pos){
	if(this->conf.motconf.enctype == EncoderType_TMC::abn && abnconf.useIndex){

		int32_t tmcpos = readReg(0x6B); // Current Position
		int32_t offset = (tmcpos - pos) % 0xffff; // Difference between current position and target

//		setup_ABN_Enc(abnconf);
		abnconf.posOffsetFromIndex += offset;
		setTmcPos(getPosAbs() - abnconf.posOffsetFromIndex);
	}else{
		setTmcPos(pos);
	}

}

/**
 * Changes position in tmc register
 */
void TMC4671::setTmcPos(int32_t pos){

	writeReg(0x6B, pos);
}

int32_t TMC4671::getPos(){

	int32_t pos = (int32_t)readReg(0x6B);
	return pos;
}

int32_t TMC4671::getPosAbs(){
	int16_t pos;
	if(this->conf.motconf.enctype == EncoderType_TMC::abn){
		pos = (int16_t)readReg(0x2A) & 0xffff; // read phiM
	}else if(this->conf.motconf.enctype == EncoderType_TMC::hall){
		pos = (int16_t)readReg(0x3A); // read phiM
	}else if(this->conf.motconf.enctype == EncoderType_TMC::sincos || this->conf.motconf.enctype == EncoderType_TMC::uvw){
		pos = (int16_t)readReg(0x46) & 0xffff; // read phiM
	}else{
		pos = getPos(); // read phiM
	}

	return pos;
}


uint32_t TMC4671::getCpr(){
//	if(this->conf.motconf.phiEsource == PhiE::abn){
//		return abnconf.cpr;
//	}else{
	if(usingExternalEncoder()){
		return drvEncoder->getCpr();
	}
	return 0xffff;
//	}

}
void TMC4671::setCpr(uint32_t cpr){
	if(cpr == 0)
		cpr = 1;


	this->abnconf.cpr = cpr;
	this->aencconf.cpr = cpr;
	writeReg(0x26, abnconf.cpr); //ABN
	writeReg(0x40, aencconf.cpr); //AENC

}

/**
 * Converts encoder counts to phiM
 */
uint32_t TMC4671::encToPos(uint32_t enc){
	return enc*(0xffff / abnconf.cpr); //*(conf.motconf.pole_pairs)
}
uint32_t TMC4671::posToEnc(uint32_t pos){
	return pos/((0xffff / abnconf.cpr)) % abnconf.cpr; //(conf.motconf.pole_pairs)
}

EncoderType TMC4671::getEncoderType(){
	if(conf.motconf.enctype == EncoderType_TMC::abn && abnconf.useIndex && encoderIndexHitFlag){
		return EncoderType::incrementalIndex;
	}
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

		setSequentialPI(true);
	}
}

void TMC4671::setSequentialPI(bool sequential){
	curPids.sequentialPI = sequential;
	updateReg(0x63, sequential ? 1 : 0, 0x1, 31);
}

void TMC4671::setExternalEncoderAllowed(bool allow){
#ifndef TIM_TMC
	allowExternalEncoder = false;
#else
	bool lastAllowed = allowExternalEncoder;
	allowExternalEncoder = allow;
	// External encoder was previously used but now not allowed anymore. Change to none type encoder
	if(!allow && lastAllowed && conf.motconf.enctype == EncoderType_TMC::ext){
		setEncoderType(EncoderType_TMC::NONE); // Reinit encoder
	}
#endif
}

bool TMC4671::externalEncoderAllowed(){
#ifndef TIM_TMC
	return false;
#else
	return allowExternalEncoder;
#endif
}

void TMC4671::setMotorType(MotorType motor,uint16_t poles){
	if(motor == MotorType::DC){
		poles = 1;
	}
	conf.motconf.motor_type = motor;
	conf.motconf.pole_pairs = poles;
	uint32_t mtype = poles | ( ((uint8_t)motor&0xff) << 16);
//	if(motor != MotorType::STEPPER){
//		maxOffsetFlux = 0; // Offsetflux only helpful for steppers. Has no effect otherwise
//	}
	writeReg(0x1B, mtype);
	if(motor == MotorType::BLDC && !ES_TMCdetected){
		setSvPwm(conf.motconf.svpwm); // Higher speed for BLDC motors. Not available in engineering samples
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
	if(curMotionMode != MotionMode::torque && !emergency){
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

/**
 * Ramps flux from current value to a target value over a specified duration
 */
void TMC4671::rampFlux(uint16_t target,uint16_t time_ms){
	uint16_t startFlux = readReg(0x64) & 0xffff;
	int32_t stepsize = (target - startFlux) / std::max<uint16_t>(1, time_ms/2);
	if(stepsize == 0){
		stepsize = startFlux < target ? 1 : -1;
	}
	uint16_t flux = startFlux;
	while(abs(target - flux) >= abs(stepsize)){
		flux+=stepsize;
		setFluxTorque(std::max<int32_t>(0,flux), 0);
		Delay(2);
	}
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

/**
 * Limits the PWM value
 */
void TMC4671::setUqUdLimit(uint16_t limit){
	this->curLimits.pid_uq_ud = limit;
	writeReg(0x5D, limit);
}

void TMC4671::setTorqueLimit(uint16_t limit){
	this->curLimits.pid_torque_flux = limit;
	bangInitPower = (float)limit*0.75;
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

/**
 * Applies a biquad filter to the flux target
 * Set nullptr to disable
 */
void TMC4671::setBiquadFlux(const TMC4671Biquad &filter){
	const TMC4671Biquad_t& bq = filter.params;
	curFilters.flux = filter;
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

/**
 * Applies a biquad filter to the pos target
 * Set nullptr to disable
 */
void TMC4671::setBiquadPos(const TMC4671Biquad &filter){
	const TMC4671Biquad_t& bq = filter.params;
	curFilters.pos = filter;
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

/**
 * Applies a biquad filter to the actual measured velocity
 * Set nullptr to disable
 */
void TMC4671::setBiquadVel(const TMC4671Biquad &filter){
	const TMC4671Biquad_t& bq = filter.params;
	curFilters.vel = filter;
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

/**
 * Applies a biquad filter to the torque target
 * Set nullptr to disable
 */
void TMC4671::setBiquadTorque(const TMC4671Biquad &filter){
	const TMC4671Biquad_t& bq = filter.params;
	curFilters.torque = filter;
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


/**
 * Changes the torque biquad filter
 */
void TMC4671::setTorqueFilter(TMC4671Biquad_conf& conf){
//	this->torqueFilter = params;
//	setBiquadTorque(makeLpTmcFilter(params,enable));
//
	// Presets: off, Lowpass, notch, peak
	this->torqueFilterConf = conf;
	TMC4671Biquad filter;
	switch(conf.mode){
	default:
	case TMCbiquadpreset::none:
		filter = TMC4671Biquad(false);
		break;
	case TMCbiquadpreset::lowpass:
		filter = TMC4671Biquad(Biquad(BiquadType::lowpass, (float)conf.params.freq / getPwmFreq(), (float)conf.params.q/100.0,0.0), true);
		break;
	case TMCbiquadpreset::notch:
		filter = TMC4671Biquad(Biquad(BiquadType::notch, (float)conf.params.freq / getPwmFreq(), (float)conf.params.q/10.0,0.0), true);
		break;
	case TMCbiquadpreset::peak:
		filter = TMC4671Biquad(Biquad(BiquadType::peak, (float)conf.params.freq / getPwmFreq(), (float)conf.params.q/10.0,conf.gain), true);
		break;
	}
	setBiquadTorque(filter);
}


/**
 *  Sets the raw brake resistor limits.
 *  Centered at 0x7fff
 *  Set both 0 to deactivate
 */
void TMC4671::setBrakeLimits(uint16_t low,uint16_t high){
	uint32_t val = low | (high << 16);
	writeReg(0x75,val);
}

/**
 * Moves the rotor and estimates polarity and direction of the encoder
 * Polarity is found by measuring the n pulse.
 * If polarity was found to be reversed during the test direction will be reversed again to account for that
 */
void TMC4671::estimateABNparams(){
	blinkClipLed(100, 0);
	int32_t pos = getPos();
	setTmcPos(0);
	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	updateReg(0x25, 0,0x1000,12); // Set dir normal
	setPhiE_ext(0);
	setPhiEtype(PhiE::ext);
	setFluxTorque(0, 0);
	setMotionMode(MotionMode::torque,true);
	rampFlux(bangInitPower, 1000);

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
		// Count how often the new position was lower than the previous indicating a reversed encoder or motor direction
		if(phiE_abn < phiE_abn_old){
			rcount++;
		}
		if((readReg(0x76) & 0x04) >> 2){
			highcount++;
		}
	}
	setTmcPos(pos+getPos());

	rampFlux(0, 100);
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);

	bool npol = highcount > c/2;
	abnconf.rdir = rcount > c/2;
	if(npol != abnconf.npol) // Invert dir if polarity was reversed TODO correct? likely wrong at the moment
		abnconf.rdir = !abnconf.rdir;

	abnconf.apol = npol;
	abnconf.bpol = npol;
	abnconf.npol = npol;
	blinkClipLed(0, 0);
}

/**
 * Similar to the ABN calibration this moved the motor and measures the encoder direction
 */
void TMC4671::estimateExtEnc(){
	blinkClipLed(100, 0);

	PhiE lastphie = getPhiEtype();
	MotionMode lastmode = getMotionMode();
	int16_t oldPhiE = getPhiE();
	setPhiE_ext(oldPhiE);
	setPhiEtype(PhiE::ext);
	setFluxTorque(0, 0);
	setMotionMode(MotionMode::torque,true);
	rampFlux(bangInitPower, 1000);
	int16_t phiE_enc = getPhiEfromExternalEncoder();
	int16_t phiE_enc_old = 0;
	int16_t rcount=0,c = 0; // Count how often direction was in reverse

	// Rotate a bit
	for(int16_t p = 0;p<0x0fff;p+=0x2f){
		setPhiE_ext(p+oldPhiE);
		Delay(10);
		c++;
		phiE_enc_old = phiE_enc;
		phiE_enc = getPhiEfromExternalEncoder();
		// Count how often the new position was lower than the previous indicating a reversed encoder or motor direction
		if(phiE_enc < phiE_enc_old){
			rcount++;
		}
	}

	rampFlux(0, 100);
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);

	if(rcount > c/2)
		conf.encoderReversed = !conf.encoderReversed;

	blinkClipLed(0, 0);
}



/**
 * Sets pwm mode: \n
 * 0 = pwm off \n
 * 1 = pwm off, HS low, LS high \n
 * 2 = pwm off, HS high, LS low \n
 * 3 = pwm off \n
 * 4 = pwm off \n
 * 5 = pwm LS only \n
 * 6 = pwm HS only \n
 * 7 = pwm on centered, FOC mode
 */
void TMC4671::setPwm(TMC_PwmMode val){
	updateReg(0x1A,(uint8_t)val,0xff,0);
}

void TMC4671::setBBM(uint8_t bbmL,uint8_t bbmH){
	this->conf.bbmH = bbmH;
	this->conf.bbmL = bbmL;
	uint32_t bbmr = bbmL | (bbmH << 8);
	writeReg(0x19, bbmr);
}

void TMC4671::setPwm(uint8_t val,uint16_t maxcnt,uint8_t bbmL,uint8_t bbmH){
	setPwmMaxCnt(maxcnt);
	setPwm((TMC_PwmMode)val);
	setBBM(bbmL, bbmH);
	writeReg(0x17,0); //Polarity
}

/**
 * Enable or disable space vector pwm for 3 phase motors
 * Normally active but should be disabled if the motor has no isolated star point
 */
void TMC4671::setSvPwm(bool enable){
	conf.motconf.svpwm = enable;
	if(conf.motconf.motor_type != MotorType::BLDC){
		enable = false; // Only valid for 3 phase motors with isolated star point
	}

	updateReg(0x1A,enable,0x01,8);
}

/**
 * Returns the PWM loop frequency in Hz
 * Depends on hardware clock and pwm counter setting. Default 25kHz
 */
float TMC4671::getPwmFreq(){
	return (4.0 * this->conf.hwconf.clockfreq) / (this->conf.pwmcnt +1);
}

/**
 * Changes PWM frequency
 * Max value 4095, minimum 255
 *
 */
void TMC4671::setPwmMaxCnt(uint16_t maxcnt){
	maxcnt = clip(maxcnt, 255, 4095);
	this->conf.pwmcnt = maxcnt;
	writeReg(0x18, maxcnt);
}

/**
 * Changes the PWM frequency to a desired frequency
 * Possible values depend on the hwclock.
 * At 25MHz the lowest possible frequency is 24.1kHz
 */
void TMC4671::setPwmFreq(float freq){
	if(freq <= 0)
		return;
	uint16_t maxcnt = ((4.0 * this->conf.hwconf.clockfreq) / freq) -1;
	setPwmMaxCnt(maxcnt);
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

/**
 * Returns measured flux and torque as a pair
 * Flux is first, torque second item
 */
std::pair<int32_t,int32_t> TMC4671::getActualTorqueFlux(){
	uint32_t tfluxa = readReg(0x69);
	int16_t af = (tfluxa & 0xffff);
	int16_t at = (tfluxa >> 16);
	return std::pair<int16_t,int16_t>(af,at);
}

/**
 * Returns measured flux
 */
int32_t TMC4671::getActualFlux(){
	uint32_t tfluxa = readReg(0x69);
	int16_t af = (tfluxa & 0xffff);
	return af;
}

/**
 * Returns measured torque
 */
int32_t TMC4671::getActualTorque(){
	uint32_t tfluxa = readReg(0x69);
	int16_t at = (tfluxa >> 16);
	return at;
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
	spiPort.transmit(spi_buf, 5, this, SPITIMEOUT);
}

void TMC4671::writeRegAsync(uint8_t reg,uint32_t dat){

	// wait until ready
	spiPort.takeSemaphore();
	spi_buf[0] = (uint8_t)(0x80 | reg);
	dat =__REV(dat);
	memcpy(spi_buf+1,&dat,4);

	// -----
#ifdef TMC4671_ALLOW_DMA
	spiPort.transmit_DMA(this->spi_buf, 5, this);
#else
	spiPort.transmit_IT(this->spi_buf, 5, this);
#endif
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
	flagCheckInProgress = true;
	statusFlags = readFlags(); // Update current flags

	// encoder index flag was set since last check. Check if the flag matching the current encoder is set
	if( (statusFlags.flags.ENC_N && this->conf.motconf.enctype == EncoderType_TMC::abn) || (statusFlags.flags.AENC_N && this->conf.motconf.enctype == EncoderType_TMC::sincos) ){
		encoderIndexHit();
	}

	if(statusFlags.flags.not_PLL_locked){
		// Critical error. PLL not locked
		// Creating error object not allowed. Function is called from flag isr! ignore for now.
		//ErrorHandler::addError(Error(ErrorCode::tmcPLLunlocked, ErrorType::critical, "TMC PLL not locked"));
	}


	setStatusFlags(0); // Reset flags
	if(readFlags().asInt != statusFlags.asInt){ // Condition is cleared. if not we will reset it in the main loop later to get out of the isr and cause some delay
		flagCheckInProgress = false;
	}
}

void TMC4671::exti(uint16_t GPIO_Pin){
	if(GPIO_Pin == FLAG_Pin && !flagCheckInProgress){ // Flag pin went high and flag check is currently not in progress (prevents interrupt flooding)
		statusCheck(); // In isr!
	}
}

void TMC4671::encoderIndexHit(){
	//pulseClipLed();
//	if(zeroEncoderOnIndexHit){
//		writeReg(0x27, 0);
//	}
	setEncoderIndexFlagEnabled(false,false); // Found the index. disable flag
	encoderIndexHitFlag = true;
}

TMC4671MotConf TMC4671::decodeMotFromInt(uint16_t val){
	// 0-2: MotType 3-5: Encoder source 6-15: Poles
	TMC4671MotConf mot;
	mot.motor_type = MotorType(val & 0x3);
	mot.svpwm = !(val & 0x4);
	mot.enctype = EncoderType_TMC( (val >> 3) & 0x7);
	mot.pole_pairs = val >> 6;
	return mot;
}
uint16_t TMC4671::encodeMotToInt(TMC4671MotConf mconf){
	uint16_t val = (uint8_t)mconf.motor_type & 0x3;
	val |= !mconf.svpwm ? 0x4 : 0;
	val |= ((uint8_t)mconf.enctype & 0x7) << 3;
	val |= (mconf.pole_pairs & 0x3FF) << 6;
	return val;
}

uint16_t TMC4671::encodeEncHallMisc(){
	uint16_t val = 0;
	val |= (this->abnconf.npol) & 0x01;
	val |= (this->conf.encoderReversed & 0x01)  << 1; // Direction


	val |= (this->abnconf.ab_as_n & 0x01) << 2;
	val |= (this->pidPrecision.current_I) << 3;
	val |= (this->pidPrecision.current_P) << 4;

	val |= (this->abnconf.useIndex) << 5;

	val |= (this->conf.combineEncoder) << 6;
	val |= (this->conf.invertForce) << 7;

	val |= ((this->conf.enableFluxDissipation & 0x01) << 8);
	val |= (this->hallconf.interpolation & 0x01) << 9;

	val |= (this->curPids.sequentialPI & 0x01) << 10;

	//11,12,13,14,15 hw version
	val |= ((uint8_t)this->conf.hwconf.hwVersion & 0x1F) << 11;

	return val;
}

void TMC4671::restoreEncHallMisc(uint16_t val){

	this->abnconf.apol = (val) & 0x01;

	this->abnconf.bpol = this->abnconf.apol;
	this->abnconf.npol = this->abnconf.apol;

	this->conf.encoderReversed = (val>>1) & 0x01;// Direction
	this->abnconf.rdir = this->conf.encoderReversed;
	this->aencconf.rdir = this->conf.encoderReversed;
	this->hallconf.direction = this->conf.encoderReversed;

	this->abnconf.ab_as_n = (val>>2) & 0x01;
	this->pidPrecision.current_I = (val>>3) & 0x01;
	this->pidPrecision.velocity_I = this->pidPrecision.current_I;
	this->pidPrecision.position_I = this->pidPrecision.current_I;
	this->pidPrecision.current_P = (val>>4) & 0x01;
	this->pidPrecision.velocity_P = this->pidPrecision.current_P;
	this->pidPrecision.velocity_P = this->pidPrecision.current_P;

	this->abnconf.useIndex = (val>>5) & 0x01;
	this->conf.combineEncoder = (val>>6) & 0x01;
	this->conf.invertForce = ((val>>7) & 0x01) && this->conf.combineEncoder;

	this->conf.enableFluxDissipation = ((val>>8) & 0x01);
	this->hallconf.interpolation = (val>>9) & 0x01;
	this->curPids.sequentialPI = (val>>10) & 0x01;

	setHwType((TMC_HW_Ver)((val >> 11) & 0x1F));

}

/**
 * Sets some constants and features depending on the hardware version of the driver
 */
void TMC4671::setHwType(TMC_HW_Ver type){
	//TMC4671HardwareTypeConf newHwConf;
	switch(type){
	case TMC_HW_Ver::v1_3_66mv:
		{
		TMC4671HardwareTypeConf newHwConf = {
			.hwVersion = TMC_HW_Ver::v1_3_66mv,
			.adcOffset = 0,
			.thermistor_R2 = 1500,
			.thermistor_R = 10000,
			.thermistor_Beta = 4300,
			.temperatureEnabled = true,
			.temp_limit = 90,
			.currentScaler = 2.5 / (0x7fff * 0.066), // sensor 66mV/A
			.brakeLimLow = 50700,
			.brakeLimHigh = 50900,
			.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
			.vSenseMult = VOLTAGE_MULT_DEFAULT,
			.bbm = 50 // DMTH8003SPS need longer deadtime
		};
		this->conf.hwconf = newHwConf;
	break;
	}
	case TMC_HW_Ver::v1_2_2_100mv:
	{
		TMC4671HardwareTypeConf newHwConf = {
			.hwVersion = TMC_HW_Ver::v1_2_2_100mv,
			.adcOffset = 0,
			.thermistor_R2 = 1500,
			.thermistor_R = 10000,
			.thermistor_Beta = 4300,
			.temperatureEnabled = true,
			.temp_limit = 90,
			.currentScaler = 2.5 / (0x7fff * 0.1), // w. TMCS1100A2 sensor 100mV/A
			.brakeLimLow = 50700,
			.brakeLimHigh = 50900,
			.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
			.vSenseMult = VOLTAGE_MULT_DEFAULT,
			.bbm = 40
		};
		this->conf.hwconf = newHwConf;
	break;
	}
	case TMC_HW_Ver::v1_2_2_LEM20:
	{
		// TODO possibly lower PWM limit because of lower valid sensor range
		TMC4671HardwareTypeConf newHwConf = {
			.hwVersion = TMC_HW_Ver::v1_2_2,
			.adcOffset = 0,
			.thermistor_R2 = 1500,
			.thermistor_R = 10000,
			.thermistor_Beta = 4300,
			.temperatureEnabled = true,
			.temp_limit = 90,
			.currentScaler = 2.5 / (0x7fff * 0.04), // w. LEM 20 sensor 40mV/A
			.brakeLimLow = 50700,
			.brakeLimHigh = 50900,
			.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
			.vSenseMult = VOLTAGE_MULT_DEFAULT,
			.bbm = 20
		};
		this->conf.hwconf = newHwConf;
	break;
	}
	case TMC_HW_Ver::v1_2_2:
	{
		// TODO possibly lower PWM limit because of lower valid sensor range
		TMC4671HardwareTypeConf newHwConf = {
			.hwVersion = TMC_HW_Ver::v1_2_2,
			.adcOffset = 0,
			.thermistor_R2 = 1500,
			.thermistor_R = 10000,
			.thermistor_Beta = 4300,
			.temperatureEnabled = true,
			.temp_limit = 90,
			.currentScaler = 2.5 / (0x7fff * 0.08), // w. LEM 10 sensor 80mV/A
			.brakeLimLow = 50700,
			.brakeLimHigh = 50900,
			.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
			.vSenseMult = VOLTAGE_MULT_DEFAULT,
			.bbm = 20
		};
		this->conf.hwconf = newHwConf;
	break;
	}

	case TMC_HW_Ver::v1_2:
	{
		TMC4671HardwareTypeConf newHwConf = {
			.hwVersion = TMC_HW_Ver::v1_2,
			.adcOffset = 1000,
			.thermistor_R2 = 1500,
			.thermistor_R = 22000,
			.thermistor_Beta = 4300,
			.temperatureEnabled = true,
			.temp_limit = 90,
			.currentScaler = 2.5 / (0x7fff * 60.0 * 0.0015), // w. 60x 1.5mOhm sensor
			.brakeLimLow = 50700,
			.brakeLimHigh = 50900,
			.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
			.vSenseMult = VOLTAGE_MULT_DEFAULT,
			.bbm = 20
		};
		this->conf.hwconf = newHwConf;
		// Activates around 60V as last resort failsave. Check offsets from tmc leakage. ~ 1.426V
	break;
	}


	case TMC_HW_Ver::v1_0:
	{
		TMC4671HardwareTypeConf newHwConf = {
			.hwVersion = TMC_HW_Ver::v1_0,
			.adcOffset = 1000,
			.thermistor_R2 = 0,
			.thermistor_R = 0,
			.thermistor_Beta = 0,
			.temperatureEnabled = false,
			.temp_limit = 90,
			.currentScaler = 2.5 / (0x7fff * 60.0 * 0.0015), // w. 60x 1.5mOhm sensor
			.brakeLimLow = 52400,
			.brakeLimHigh = 52800,
			.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
			.vSenseMult = VOLTAGE_MULT_DEFAULT,
			.bbm = 20
		};
		this->conf.hwconf = newHwConf;

	break;
	}

	case TMC_HW_Ver::NONE:
	{
	default:
		TMC4671HardwareTypeConf newHwConf;
		newHwConf.temperatureEnabled = false;
		newHwConf.hwVersion = TMC_HW_Ver::NONE;
		newHwConf.currentScaler = 0;
		this->conf.hwconf = newHwConf;
		setBrakeLimits(0,0); // Disables internal brake resistor activation. DANGER!
		break;
	}
	}
	setVSenseMult(this->conf.hwconf.vSenseMult); // Update vsense multiplier
	//setupBrakePin(vdiffAct, vdiffDeact, vMax); // TODO if required
	setBrakeLimits(this->conf.hwconf.brakeLimLow,this->conf.hwconf.brakeLimHigh);
	setBBM(this->conf.hwconf.bbm,this->conf.hwconf.bbm);

}

void TMC4671::registerCommands(){
	CommandHandler::registerCommands();

	registerCommand("cpr", TMC4671_commands::cpr, "CPR in TMC",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("mtype", TMC4671_commands::mtype, "Motor type",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("encsrc", TMC4671_commands::encsrc, "Encoder source",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("tmcHwType", TMC4671_commands::tmcHwType, "Version of TMC board",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("encalign", TMC4671_commands::encalign, "Align encoder",CMDFLAG_GET);
	registerCommand("poles", TMC4671_commands::poles, "Motor pole pairs",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("acttrq", TMC4671_commands::acttrq, "Measure torque and flux",CMDFLAG_GET);
	registerCommand("pwmlim", TMC4671_commands::pwmlim, "PWM limit",CMDFLAG_DEBUG | CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("torqueP", TMC4671_commands::torqueP, "Torque P",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("torqueI", TMC4671_commands::torqueI, "Torque I",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("fluxP", TMC4671_commands::fluxP, "Flux P",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("fluxI", TMC4671_commands::fluxI, "Flux I",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("velocityP", TMC4671_commands::velocityP, "Velocity P",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("velocityI", TMC4671_commands::velocityI, "Velocity I",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("posP", TMC4671_commands::posP, "Pos P",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("posI", TMC4671_commands::posI, "Pos I",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("tmctype", TMC4671_commands::tmctype, "Version of TMC chip",CMDFLAG_GET);
	registerCommand("pidPrec", TMC4671_commands::pidPrec, "PID precision bit0=I bit1=P. 0=Q8.8 1= Q4.12",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("phiesrc", TMC4671_commands::phiesrc, "PhiE source",CMDFLAG_DEBUG | CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("fluxoffset", TMC4671_commands::fluxoffset, "Offset flux scale for field weakening",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("seqpi", TMC4671_commands::seqpi, "Sequential PI",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("iScale", TMC4671_commands::tmcIscale, "Counts per A",CMDFLAG_STR_ONLY);
	registerCommand("encdir", TMC4671_commands::encdir, "Encoder dir",CMDFLAG_DEBUG | CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("abnpol", TMC4671_commands::encpol, "Encoder polarity",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("temp", TMC4671_commands::temp, "Temperature in C * 100",CMDFLAG_GET);
	registerCommand("reg", TMC4671_commands::reg, "Read or write a TMC register at adr",CMDFLAG_DEBUG | CMDFLAG_GETADR | CMDFLAG_SETADR);
	registerCommand("svpwm", TMC4671_commands::svpwm, "Space-vector PWM",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("autohome", TMC4671_commands::findIndex, "Find abn index",CMDFLAG_GET);
	registerCommand("abnindex", TMC4671_commands::abnindexenabled, "Enable ABN index",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("calibrate", TMC4671_commands::fullCalibration, "Full calibration",CMDFLAG_GET);
	registerCommand("calibrated", TMC4671_commands::calibrated, "Calibration valid",CMDFLAG_GET);
	registerCommand("state", TMC4671_commands::getState, "Get state",CMDFLAG_GET);
	registerCommand("combineEncoder", TMC4671_commands::combineEncoder, "Use TMC for movement. External encoder for position",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("invertForce", TMC4671_commands::invertForce, "Invert incoming forces",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("vm", TMC4671_commands::vmTmc, "VM in mV",CMDFLAG_GET);
	registerCommand("extphie", TMC4671_commands::extphie, "external phie",CMDFLAG_GET);
	registerCommand("trqbq_mode", TMC4671_commands::torqueFilter_mode, "Torque filter mode: none;lowpass;notch;peak",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("trqbq_f", TMC4671_commands::torqueFilter_f, "Torque filter freq 1000 max. 0 to disable. (Stored f/2)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("trqbq_q", TMC4671_commands::torqueFilter_q, "Torque filter q*100",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("pidautotune", TMC4671_commands::pidautotune, "Start PID autoruning",CMDFLAG_GET);
	registerCommand("fluxbrake", TMC4671_commands::fluxbrake, "Prefer energy dissipation in motor",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("pwmfreq", TMC4671_commands::pwmfreq, "Get/set pwm frequency",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_DEBUG);
}


CommandStatus TMC4671::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	CommandStatus status = CommandStatus::OK;
	switch(static_cast<TMC4671_commands>(cmd.cmdId)){
	case TMC4671_commands::combineEncoder:
		status = handleGetSet(cmd, replies, this->conf.combineEncoder);
		if(!this->conf.combineEncoder){
			this->conf.invertForce = false; // Force off
		}

		break;

	case TMC4671_commands::cpr:
		handleGetFuncSetFunc(cmd, replies, &TMC4671::getEncCpr, &TMC4671::setCpr, this);
	break;

	case TMC4671_commands::invertForce:
		handleGetSet(cmd, replies, conf.invertForce);
		break;

	case TMC4671_commands::getState:
		replies.emplace_back((uint32_t)getState());
		break;

	case TMC4671_commands::fullCalibration:
		calibrationFailCount = 1; // allow 1 fail
		changeState(TMC_ControlState::FullCalibration);
		// TODO start full calibration and save in flash
		break;

	case TMC4671_commands::calibrated:
		replies.emplace_back(!recalibrationRequired && adcCalibrated);
		break;

	case TMC4671_commands::mtype:
		if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->conf.motconf.motor_type);
		}else if(cmd.type == CMDtype::set && (uint8_t)cmd.type < (uint8_t)MotorType::ERR){
			this->setMotorType((MotorType)cmd.val, this->conf.motconf.pole_pairs);
		}else{
			replies.emplace_back("NONE=0,DC=1,2Ph Stepper=2,3Ph BLDC=3");
		}
		break;

	case TMC4671_commands::encsrc:
		if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->conf.motconf.enctype);
		}else if(cmd.type == CMDtype::set){
			this->setEncoderType((EncoderType_TMC)cmd.val);
		}else{
			if(externalEncoderAllowed())
				replies.emplace_back("NONE=0,ABN=1,SinCos=2,Analog UVW=3,Hall=4,External=5");
			else
				replies.emplace_back("NONE=0,ABN=1,SinCos=2,Analog UVW=3,Hall=4");

		}
		break;

	case TMC4671_commands::tmcHwType:
		if(cmd.type == CMDtype::get){
			replies.push_back((uint8_t)conf.hwconf.hwVersion);
		}else if(cmd.type == CMDtype::set){
			if(conf.canChangeHwType)
				setHwType((TMC_HW_Ver)(cmd.val & 0x1F));
		}else{
			// List known hardware versions
			for(auto v : tmcHwVersionNames){
				if(conf.canChangeHwType || v.first == conf.hwconf.hwVersion){
					replies.emplace_back( std::to_string((uint8_t)v.first) + ":" + v.second,(uint8_t)v.first);
				}

			}
		}
		break;

	case TMC4671_commands::encalign:
		if(cmd.type == CMDtype::get){
			encoderAligned = false;
			this->setEncoderType(this->conf.motconf.enctype);
			manualEncAlign = true;
			return CommandStatus::NO_REPLY;
		}else{
			return CommandStatus::ERR;
		}
		break;

	case TMC4671_commands::poles:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->conf.motconf.pole_pairs);
		}else if(cmd.type == CMDtype::set){
			this->setMotorType(this->conf.motconf.motor_type,cmd.val);
		}
		break;

	case TMC4671_commands::acttrq:
		if(cmd.type == CMDtype::get){
			std::pair<int32_t,int32_t> current = getActualTorqueFlux();
			replies.emplace_back(current.second,current.first);
		}
		break;

	case TMC4671_commands::pwmlim:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->curLimits.pid_uq_ud);
		}else if(cmd.type == CMDtype::set){
			this->setUqUdLimit(cmd.val);
		}
		break;

	case TMC4671_commands::torqueP:
		handleGetSet(cmd, replies, this->curPids.torqueP);
		if(cmd.type == CMDtype::set)
			setPids(curPids);
		break;

	case TMC4671_commands::torqueI:
		handleGetSet(cmd, replies, this->curPids.torqueI);
		if(cmd.type == CMDtype::set)
			setPids(curPids);
		break;

	case TMC4671_commands::fluxP:
		handleGetSet(cmd, replies, this->curPids.fluxP);
		if(cmd.type == CMDtype::set)
			setPids(curPids);
		break;

	case TMC4671_commands::fluxI:
		handleGetSet(cmd, replies, this->curPids.fluxI);
		if(cmd.type == CMDtype::set)
			setPids(curPids);
		break;

	case TMC4671_commands::velocityP:
		handleGetSet(cmd, replies, this->curPids.velocityP);
		if(cmd.type == CMDtype::set)
			setPids(curPids);
		break;

	case TMC4671_commands::velocityI:
		handleGetSet(cmd, replies, this->curPids.velocityI);
		if(cmd.type == CMDtype::set)
			setPids(curPids);
		break;

	case TMC4671_commands::posP:
		handleGetSet(cmd, replies, this->curPids.positionP);
		if(cmd.type == CMDtype::set)
			setPids(curPids);
		break;

	case TMC4671_commands::posI:
		handleGetSet(cmd, replies, this->curPids.positionI);
		if(cmd.type == CMDtype::set)
			setPids(curPids);
		break;

	case TMC4671_commands::abnindexenabled:
		handleGetSet(cmd, replies, this->abnconf.useIndex);
		if(cmd.type == CMDtype::set)
			setup_ABN_Enc(abnconf);
		break;

	case TMC4671_commands::tmctype:
	{
		std::pair<uint32_t,std::string> ver = getTmcType();
		replies.emplace_back(ver.second,ver.first);
		break;
	}

	case TMC4671_commands::vmTmc:
		replies.emplace_back(getTmcVM());
		break;

	case TMC4671_commands::pidPrec:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->pidPrecision.current_I | (this->pidPrecision.current_P << 1));
		}else if(cmd.type == CMDtype::set){
			this->pidPrecision.current_I = cmd.val & 0x1;
			this->pidPrecision.current_P = (cmd.val >> 1) & 0x1;
			this->setPidPrecision(pidPrecision);
		}
		break;
	case TMC4671_commands::phiesrc:
		if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->getPhiEtype());
		}else if(cmd.type == CMDtype::set){
			this->setPhiEtype((PhiE)cmd.val);
		}else{
			replies.emplace_back("ext=1,openloop=2,abn=3,hall=5,aenc=6,aencE=7");
		}
		break;
	case TMC4671_commands::fluxoffset:
		handleGetSet(cmd, replies, maxOffsetFlux);
		break;
	case TMC4671_commands::seqpi:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->curPids.sequentialPI);
		}else if(cmd.type == CMDtype::set){
			this->setSequentialPI(cmd.val != 0);
		}
		break;
	case TMC4671_commands::tmcIscale:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(std::to_string(this->conf.hwconf.currentScaler)); // TODO float as value?
		}
		break;
	case TMC4671_commands::encdir:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->abnconf.rdir);
		}else if(cmd.type == CMDtype::set){
			this->abnconf.rdir = cmd.val != 0;
			this->setup_ABN_Enc(this->abnconf);
		}
		break;

	case TMC4671_commands::encpol:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->abnconf.npol);
		}else if(cmd.type == CMDtype::set){
			this->abnconf.npol = cmd.val != 0;
			this->abnconf.apol = cmd.val != 0;
			this->abnconf.bpol = cmd.val != 0;
			this->setup_ABN_Enc(this->abnconf);
		}
		break;
	case TMC4671_commands::temp:
		if(cmd.type == CMDtype::get){
			replies.emplace_back((int32_t)(this->getTemp()*100.0));
		}
		break;
	case TMC4671_commands::reg:
		if(cmd.type == CMDtype::getat){
			replies.emplace_back(readReg(cmd.val));
		}else if(cmd.type == CMDtype::setat){
			writeReg(cmd.adr,cmd.val);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case TMC4671_commands::findIndex:
		changeState(TMC_ControlState::IndexSearch);
		break;

	case TMC4671_commands::svpwm:
	{
		if(cmd.type == CMDtype::set){
			setSvPwm(cmd.val != 0);
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(conf.motconf.svpwm);
		}
		break;
	}
	case TMC4671_commands::extphie:
	{

		replies.emplace_back(getPhiEfromExternalEncoder());

		break;
	}
	case TMC4671_commands::torqueFilter_mode:
		if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->torqueFilterConf.mode);
		}else if(cmd.type == CMDtype::set && (uint8_t)cmd.val < 4){
			torqueFilterConf.mode = (TMCbiquadpreset)(cmd.val);
			this->setTorqueFilter(torqueFilterConf);
		}else{
			replies.emplace_back("OFF=0,Lowpass=1,Notch=2,Peak=3");
		}
		break;
	case TMC4671_commands::torqueFilter_f:
	{
		if(cmd.type == CMDtype::set){
				torqueFilterConf.params.freq = clip(cmd.val,1,0x1fff);
				this->setTorqueFilter(torqueFilterConf);
			}else if(cmd.type == CMDtype::get){
				replies.emplace_back(torqueFilterConf.params.freq);
			}
		break;
	}

	case TMC4671_commands::torqueFilter_q:
		if(cmd.type == CMDtype::set){
			torqueFilterConf.params.q = clip(cmd.val,0,127);
				this->setTorqueFilter(torqueFilterConf);
			}else if(cmd.type == CMDtype::get){
				replies.emplace_back(torqueFilterConf.params.q);
			}
		break;
	case TMC4671_commands::pidautotune:
		changeState(TMC_ControlState::Pidautotune);
		return CommandStatus::NO_REPLY;

	case TMC4671_commands::fluxbrake:
		handleGetSet(cmd, replies, conf.enableFluxDissipation);
		break;

	case TMC4671_commands::pwmfreq:
		if(cmd.type == CMDtype::set){
				setPwmFreq(cmd.val);
			}else if(cmd.type == CMDtype::get){
				replies.emplace_back(getPwmFreq());
			}
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}

	return status;


}

#ifdef TIM_TMC
	void TMC4671::timerElapsed(TIM_HandleTypeDef* htim){
		if(htim != this->externalEncoderTimer){
			return;
		}
		// Read encoder and send to tmc
		if(usingExternalEncoder() && externalEncoderAllowed() && this->conf.motconf.phiEsource == PhiE::extEncoder && extEncUpdater != nullptr){
			//setPhiE_ext(getPhiEfromExternalEncoder());
			// Signal phiE update
			extEncUpdater->updateFromIsr(); // Use task so that the update is not being done inside an ISR
		}
	}
#endif

void TMC4671::setUpExtEncTimer(){
#ifdef TIM_TMC
	if(extEncUpdater == nullptr) // Create updater thread
		extEncUpdater = std::make_unique<TMC_ExternalEncoderUpdateThread>(this);
	// Setup timer
	this->externalEncoderTimer = &TIM_TMC;
	this->externalEncoderTimer->Instance->ARR = 200; // 200 = 5khz = 5 tmc cycles, 250 = 4khz, 240 = 6 tmc cycles
	this->externalEncoderTimer->Instance->PSC = (SystemCoreClock / 2000000)+1; // timer running at half clock speed. 1µs ticks
	this->externalEncoderTimer->Instance->CR1 = 1;
	HAL_TIM_Base_Start_IT(this->externalEncoderTimer);
#endif
}

/**
 * Medium priority task to update external encoders
 */
TMC4671::TMC_ExternalEncoderUpdateThread::TMC_ExternalEncoderUpdateThread(TMC4671* tmc) : cpp_freertos::Thread("TMCENC",80,33),tmc(tmc){
	this->Start();
}

void TMC4671::TMC_ExternalEncoderUpdateThread::Run(){
	while(true){
		this->WaitForNotification();
		if(tmc->usingExternalEncoder() && !tmc->spiPort.isTaken()){
			tmc->writeRegAsync(0x1C, (tmc->getPhiEfromExternalEncoder())); // Write phiE_ext
		}
	}
}

void TMC4671::TMC_ExternalEncoderUpdateThread::updateFromIsr(){
	if(tmc->initialized)
		this->NotifyFromISR();
}

void TMC4671::errorCallback(const Error &error, bool cleared){
	if(!cleared && error.code == ErrorCode::brakeResistorFailure){
		// shut down and block.
		emergencyStop(false);
		this->changeState(TMC_ControlState::HardError, true);
	}
}
#endif
