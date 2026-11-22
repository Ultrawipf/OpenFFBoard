/*
 * TMC4671.cpp
 *
 *  Created on: Feb 1, 2020
 *	  Author: Yannick, Vincent
 */

#include "TMC4671.h"
#include <cstdarg>
#include <new>
#ifdef TMC4671DRIVER
#include "ledEffects.h"
#include "voltagesense.h"
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
	this->drv_address = address;
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
	spiConfig.peripheral.BaudRatePrescaler = spiPort.getClosestPrescaler(8e6,0,10e6).first; // 8 target, 10MHz max
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
	// Safely stop hardware timers, external encoder thread, PWM, and enable pin before base destruction.
	allowExternalEncoder = false;
	stopCalibTimers();
#ifdef TIM_TMC
	if(externalEncoderTimer != nullptr){
		HAL_TIM_Base_Stop_IT(externalEncoderTimer);
	}
#endif
	extEncUpdater.reset();
	disablePowerStage();
}


const ClassIdentifier TMC4671::getInfo() {

	return info;
}


void TMC4671::setAddress(uint8_t address){
	if (address == 1){
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC1_MOTCONF, ADR_TMC1_CPR, ADR_TMC1_ENCA, ADR_TMC1_OFFSETFLUX, ADR_TMC1_TORQUE_P, ADR_TMC1_TORQUE_I, ADR_TMC1_FLUX_P, ADR_TMC1_FLUX_I,ADR_TMC1_ADC_I0_OFS,ADR_TMC1_ADC_I1_OFS,ADR_TMC1_ENC_OFFSET,ADR_TMC1_PHIE_OFS,ADR_TMC1_TRQ_FILT
#ifdef COGGING_TABLE_FLASH_START_ADDRESS
			,ADR_TMC1_COGGING_CAL
#endif
			});
	}else if (address == 2)
	{
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC2_MOTCONF, ADR_TMC2_CPR, ADR_TMC2_ENCA, ADR_TMC2_OFFSETFLUX, ADR_TMC2_TORQUE_P, ADR_TMC2_TORQUE_I, ADR_TMC2_FLUX_P, ADR_TMC2_FLUX_I,ADR_TMC2_ADC_I0_OFS,ADR_TMC2_ADC_I1_OFS,ADR_TMC2_ENC_OFFSET,ADR_TMC2_PHIE_OFS,ADR_TMC2_TRQ_FILT
#ifdef COGGING_TABLE_FLASH_START_ADDRESS
	,ADR_TMC2_COGGING_CAL
#endif
		});
	}else if (address == 3)
	{
		this->flashAddrs = TMC4671FlashAddrs({ADR_TMC3_MOTCONF, ADR_TMC3_CPR, ADR_TMC3_ENCA, ADR_TMC3_OFFSETFLUX, ADR_TMC3_TORQUE_P, ADR_TMC3_TORQUE_I, ADR_TMC3_FLUX_P, ADR_TMC3_FLUX_I,ADR_TMC3_ADC_I0_OFS,ADR_TMC3_ADC_I1_OFS,ADR_TMC3_ENC_OFFSET,ADR_TMC3_PHIE_OFS,ADR_TMC3_TRQ_FILT
#ifdef COGGING_TABLE_FLASH_START_ADDRESS			
	,ADR_TMC3_COGGING_CAL
#endif
		});
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

#ifdef COGGING_TABLE_FLASH_START_ADDRESS
	// Save cogging state
	uint16_t coggingFlash = (cogging_enabled ? 1 : 0);
	Flash_Write(flashAddrs.coggingEnable, coggingFlash);
	
	int16_t scale_int = clip<float>(this->cogging_scale * 10000.0f, -32767, 32767);
	Flash_Write(flashAddrs.coggingScale, (uint16_t)scale_int);
#endif
}

/**
 * Writes ADC offsets into flash
 */
void TMC4671::saveAdcParams(){
	Flash_Write(flashAddrs.ADC_i0_ofs, conf.adc_I0_offset);
	Flash_Write(flashAddrs.ADC_i1_ofs, conf.adc_I1_offset);
	adcSettingsStored = true;
}

#ifdef COGGING_TABLE_FLASH_START_ADDRESS
void TMC4671::saveCoggingTable(){
	Flash_WriteCoggingTable(this->drv_address - 1, (void*)this->cogging_harmonics);
}

void TMC4671::clearCoggingTable(){
	memset(cogging_harmonics, 0, sizeof(cogging_harmonics));
	cogging_enabled = false;
	saveCoggingTable();
}
#endif

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
	}else{
		// set first hwconf if we can't restore
		this->setHwType(TMC4671::tmc4671_hw_configs[0].hwVersion);
	}
	uint16_t filterval;
	if(Flash_Read(flashAddrs.torqueFilter, &filterval)){
		torqueFilterConf.params.freq = filterval & 0x1fff;
		torqueFilterConf.mode = static_cast<TMCbiquadpreset>((filterval >> 13) & 0x7);
	}

#ifdef COGGING_TABLE_FLASH_START_ADDRESS
	uint16_t cogging = 0; // Initialize to avoid random stack data
	if(Flash_Read(flashAddrs.coggingEnable, &cogging)) {
		cogging_enabled = cogging & 0x01;
	} else {
		cogging_enabled = false;
	}
	
	uint16_t scale_flash = 0;
	if(Flash_Read(flashAddrs.coggingScale, &scale_flash)) {
		int16_t scale_int = (int16_t)scale_flash;
		this->cogging_scale = (float)scale_int / 10000.0f;
	} else {
		this->cogging_scale = 0.0f;
	}

	Flash_ReadCoggingTable(this->drv_address - 1, (int16_t*)this->cogging_harmonics);
	// Sanitize legacy flash tables where the former 2-byte padding (now speed_delta) may contain 0xFFFF (-1) or garbage
	for (uint8_t i = 0; i < COGGING_HARMONICS_COUNT; i++) {
		if (!std::isfinite(cogging_harmonics[i].amplitude) || !std::isfinite(cogging_harmonics[i].phase) ||
			cogging_harmonics[i].amplitude < 0.0f || cogging_harmonics[i].amplitude > 32767.0f ||
			cogging_harmonics[i].order >= COGGING_CALIB_DFT_HARMONICS) {
			cogging_harmonics[i] = {0.0f, 0.0f, 0, 0};
		} else {
			int32_t max_delta = (int32_t)(cogging_harmonics[i].amplitude * 2.0f);
			int32_t min_delta = -(int32_t)(cogging_harmonics[i].amplitude * 0.8f);
			cogging_harmonics[i].speed_delta = (int16_t)clip<int32_t, int32_t>(cogging_harmonics[i].speed_delta, min_delta, max_delta);
		}
	}
#endif
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

void TMC4671::setupDriver() {
	// Configuration specific to TMC4671 upon starting the motor
	// The power limit is set by the Axis class via setPowerLimit during initialization.

	this->setExternalEncoderAllowed(true);
	this->restoreFlash();
	this->setLimits(curLimits);

	// Start driver
	this->setMotionMode(MotionMode::torque);
	this->Start(); // Start thread
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
		ErrorHandler::addError(COMMUNICATION_ERROR);
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

		this->spiConfig.peripheral.BaudRatePrescaler = spiPort.getClosestPrescaler(1e6).first; // 1MHz target
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
	setBiquadFlux(TMC4671Biquad(Biquad(BiquadType::lowpass, FLUX_FILTER_FREQ / getPwmFreq(), 0.7,0.0), true)); // Create flux filter

	// Initial adc calibration and check without PWM if power off to get basic offsets. PWM is off!
	if(!hasPower()){
		if(!calibrateAdcOffset(150)){
			enterHardError(); // ADC or shunt amp is broken!
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
	if(!this->conf.hwconf.thermistorSettings.temperatureEnabled){
		return 0;
	}
	TMC4671HardwareTypeConf* hwconf = &conf.hwconf;

	writeReg(0x03, 2);
	int32_t adcval = ((readReg(0x02)) & 0xffff) - 0x7fff; // Center offset
	adcval -= hwconf->adcOffset;
	if(adcval <= 0){
		return 0.0;
	}
	float r = hwconf->thermistorSettings.thermistor_R2 * (((float)43252 / (float)adcval)); //43252 equivalent ADC count if it was 3.3V and not 2.5V

	// Beta
	r = (1.0 / 298.15) + log(r / hwconf->thermistorSettings.thermistor_R) / hwconf->thermistorSettings.thermistor_Beta;
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

bool TMC4671::initializeWithPower(){
	if(powerInitialized){
		return true;
	}
	if(!canRunMotor()){
		return false;
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
			return false; // Abort
		}
	}

	ErrorHandler::clearError(ErrorCode::undervoltage);

	// Enable power stage only when a motor and encoder are configured and no fault is active.
	if(conf.motconf.motor_type != MotorType::NONE && conf.motconf.enctype != EncoderType_TMC::NONE && canRunMotor()){
		setFluxTorque(0, 0);
		enablePin.set();
		setPwm(TMC_PwmMode::PWM_FOC); // enable foc
	}
	if(!encoderAligned){
		setEncoderType(conf.motconf.enctype);
	}else if(canRunMotor()){
		//last state
		allowStateChange = true;
		changeState(requestedState);
		setMotionMode(lastMotionMode,true);
	}
	return true;
}

bool TMC4671::motorReady(){
	return initialized && !isEmergencyActive() && !brake_failure && conf.motconf.motor_type != MotorType::NONE && this->state == TMC_ControlState::Running && powerInitialized && adcCalibrated && encoderAligned;
}

void TMC4671::Run(){
	// Main state machine
	while(1){

		// Immediately shut down and latch HardError if the brake resistor protection tripped.
		if(brake_failure && state != TMC_ControlState::HardError){
			emergencyStop(false);
			ErrorHandler::addError(BRAKE_RES_ERROR);
			enterHardError();
		}

		if(!initialized && state != TMC_ControlState::HardError){
			changeState(TMC_ControlState::uninitialized,true);
		}

		// Force immediate hardware cutoff and Shutdown state whenever emergency stop is active.
		if(isEmergencyActive() && state != TMC_ControlState::HardError && state != TMC_ControlState::Shutdown){
			enterShutdown();
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
			handleStateWaitPower();
			break;

		case TMC_ControlState::FullCalibration:
			handleStateFullCalibration();
			break;

		case TMC_ControlState::Pidautotune:
			handleStatePidAutoTune();
			break;

#ifdef COGGING_TABLE_FLASH_START_ADDRESS
		case TMC_ControlState::CoggingCalibration:
			handleStateCoggingCalibration();
			break;
#endif
		case TMC_ControlState::IndexSearch:
			if(!canRunMotor() || conf.motconf.motor_type == MotorType::NONE){
				changeState(isEmergencyActive() ? TMC_ControlState::Shutdown : laststate);
				break;
			}
			allowStateChange = false;
			autohome();
			allowStateChange = true;
			if(isEmergencyActive() || brake_failure){
				enterShutdown();
			}else if(motorEnabledRequested && isSetUp()){
				changeState(TMC_ControlState::Running);
			}else{
				stopMotor();
				changeState(TMC_ControlState::Shutdown);
			}
			break;

		case TMC_ControlState::Running:
			handleStateRunning();
			break;

		case TMC_ControlState::Shutdown:
			// Ensure hardware power stage and FOC PWM remain disabled in Shutdown.
			if(curMotionMode != MotionMode::stop){
				disablePowerStage(true);
			}else{
				enablePin.reset();
			}
			Delay(100);
			if(estopTriggered){
				if(isHardwareEnableHigh()){
					// Recover from external hardware enable pin emergency stop when pin is restored.
					ErrorHandler::clearError(ESTOP_ERROR);
					this->estopTriggered = false;
					this->emergency = false;
					changeState(TMC_ControlState::uninitialized,true);
				}
			}else if(!emergency && motorEnabledRequested && conf.motconf.motor_type != MotorType::NONE){
				// Resume motor operation safely from thread context after an ISR E-stop release.
				startMotor();
			}
			break;

		case TMC_ControlState::EncoderInit:
			if(powerInitialized && hasPower() && conf.motconf.motor_type != MotorType::NONE)
				encoderInit();
		break;

		case TMC_ControlState::ExternalEncoderInit:
			if(powerInitialized && hasPower() && drvEncoder != nullptr && conf.motconf.motor_type != MotorType::NONE)
				encoderInit();
			break;

		case TMC_ControlState::HardError:
			// Unconditionally keep power stage disabled and state locked in HardError.
			enterHardError();
			Delay(250);
		break; // Broken

		case TMC_ControlState::OverTemp:
			// Immediately disable power stage and latch HardError on over-temperature fault.
			enterHardError();
		break;

		case TMC_ControlState::EncoderFinished: // Startup sequence done
			encoderAligned = true;
			if(fullCalibrationInProgress){
				// Proceed directly to slew-rate calibration before cutting PWM or entering Running.
				Flash_Write(flashAddrs.encA,encodeEncHallMisc()); // Save encoder settings
				changeState(TMC_ControlState::SlewRateCalibration);
			}else if(motorEnabledRequested && isSetUp()){
				startMotor();
				changeState(TMC_ControlState::Running);
			}else{
				stopMotor();
				laststate = TMC_ControlState::Running; // Go to running when starting again
			}
		break;

		default:

		break;

		case TMC_ControlState::SlewRateCalibration:
			{
				if(isEmergencyActive() || brake_failure){
					fullCalibrationInProgress = false;
					enterShutdown();
					break;
				}
				if(!hasPower()){
					this->postPowerState = TMC_ControlState::SlewRateCalibration;
					changeState(TMC_ControlState::waitPower);
					break;
				}
				allowStateChange = false;
				measureMaxSlewRate();
				allowStateChange = true;
				if(isEmergencyActive() || brake_failure){
					fullCalibrationInProgress = false;
					enterShutdown();
				}else if(fullCalibrationInProgress){
					fullCalibrationInProgress = false;
					if(motorEnabledRequested && isSetUp()){
						startMotor();
						changeState(TMC_ControlState::Running);
					}else{
						stopMotor();
						laststate = TMC_ControlState::Running; // Go to running when starting again
					}
				}else if(motorEnabledRequested && isSetUp()){
					changeState(TMC_ControlState::Running);
				}else{
					stopMotor();
					changeState(TMC_ControlState::Shutdown);
				}
				break;
			}
		}


		// Optional update methods for safety

		if(!hasPower() && state != TMC_ControlState::waitPower && state != TMC_ControlState::CoggingCalibration && state != TMC_ControlState::HardError && initialized && powerInitialized){ // low voltage or overvoltage

			requestedState = state;
			ErrorHandler::addError(LOW_VOLTAGE_ERROR);
			// Immediately cut current, PWM, and enable pin on undervoltage or overvoltage.
			disablePowerStage();
			changeState(TMC_ControlState::waitPower,true);
			allowStateChange = false;
		}

		if(flagCheckPending && initialized){
			// Process pending FLAG_Pin status check safely in thread context with SPI semaphore protection.
			flagCheckPending = false;
			statusCheck();
		}

		if(flagCheckInProgress){ // cause some delay until reenabling the status interrupt checking
			setStatusFlags(0);
			flagCheckInProgress = false;
		}
		Delay(10);
	} // End while
}
void TMC4671::calibrateEncoder(){
	if(!canRunMotor()){
		return;
	}
	if(conf.motconf.enctype == EncoderType_TMC::abn) {
		estimateABNparams();
		// Report changes
		CommandHandler::broadcastCommandReply(CommandReply(abnconf.npol ? 1 : 0), (uint32_t)TMC4671_commands::encpol, CMDtype::get);
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		if(!conf.hwconf.flags.analog_enc_skip_cal){
			calibrateAenc();
		}
	}else if(conf.motconf.enctype == EncoderType_TMC::ext){
		estimateExtEnc();
	}
	if(canRunMotor()){
		changeState(conf.motconf.enctype == EncoderType_TMC::ext ? TMC_ControlState::ExternalEncoderInit : TMC_ControlState::EncoderInit);
	}
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

void TMC4671::changeState(TMC_ControlState newState,bool force){
	// Never leave HardError once latched until MCU reset.
	if(this->state == TMC_ControlState::HardError){
		return;
	}
	if(newState != this->state){
		this->laststate = this->state; // save last state if new state wants to jump back
	}
	if(newState == TMC_ControlState::HardError){
		// Permanently latch HardError across both state and requestedState to prevent overwriting.
		this->allowStateChange = false;
		this->requestedState = TMC_ControlState::HardError;
		this->state = TMC_ControlState::HardError;
		return;
	}
	if(!force){
		this->requestedState = newState;
	}else{
		// Lock state transitions when forcing privileged boot or power-wait states so requestedState cannot overwrite them.
		if(newState == TMC_ControlState::uninitialized || newState == TMC_ControlState::waitPower){
			this->allowStateChange = false;
		}
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
	// Guard against zero CPR and use 64-bit scaling to avoid integer overflow on high-CPR encoders.
	if(abnconf.cpr == 0){
		return;
	}
	int32_t npos = (int32_t)readReg(0x28); // raw encoder counts at index hit
	int32_t npos_M = (int32_t)(((int64_t)npos * 0xffff) / (int64_t)abnconf.cpr); // Scaled encoder angle at index
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

	if(conf.motconf.enctype != EncoderType_TMC::abn || !enablePowerStageForCalibration()){
		return false; // Only valid for ABN encoders when power stage is safe to energize
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
	while(!encoderIndexHitFlag && timeout-- > 0 && canRunMotor()){
		if(flagCheckPending){
			// Poll latched status flags in thread context when FLAG_Pin interrupt fires.
			flagCheckPending = false;
			statusCheck();
		}
		Delay(10);
	}
	//int32_t speed = 10;
	rampFlux(0, 100);
	runOpenLoop(0, 0, 0, 10, true);
	if(isEmergencyActive()){
		setEncoderIndexFlagEnabled(false, false);
		setFluxTorque(0, 0);
		return false;
	}
	if(!encoderIndexHitFlag){
		pulseErrLed();
		ErrorHandler::addError(INDEX_NOT_HIT_ERROR);
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
	// Block position commands when emergency stop is active.
	if(isEmergencyActive()){
		return;
	}
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
	// Block velocity commands when emergency stop is active.
	if(isEmergencyActive()){
		return;
	}
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
	// Guard against null external encoder or zero CPR to prevent division-by-zero HardFault.
	if(drvEncoder == nullptr){
		return 0;
	}
	uint32_t ext_cpr = drvEncoder->getCpr();
	if(ext_cpr == 0){
		return 0;
	}
	int64_t phiE_t = (int64_t)drvEncoder->getPosAbs() * 0xffff;
	if(this->conf.encoderReversed){
		phiE_t = -phiE_t;
	}
	int32_t phiE = (int32_t)(phiE_t / (int64_t)ext_cpr);
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
	if(!canRunMotor() || (this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC)){ // If not stepper or bldc return
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

	int16_t phiEpos = getPhiE();// readReg(phiEreg)>>16; // starts at current encoder position
	if(phiEoffsetReg != 0){
		updateReg(phiEoffsetReg, 0, 0xffff, 16); // Set phiE offset to zero
	}
	setPhiE_ext(phiEpos);
	setPhiEtype(PhiE::ext);
	// Ramp up flux
	rampFlux(power, 1000);
	int16_t phiE_enc = getPhiE_Enc();

	Delay(50);
	int16_t phiE_abn_old = 0;
	int16_t c = 0;
	uint16_t still = 0;
	while(still < 30 && c++ < 1000 && canRunMotor()){
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
	if(!canRunMotor()){
		setFluxTorque(0, 0);
		setPhiE_ext(0);
		setPhiEtype(lastphie);
		blinkClipLed(0, 0);
		return;
	}

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
	uint32_t aencStartTick = HAL_GetTick();
	auto applyAencOffsetScale = [&](uint32_t s0, uint32_t s1, uint32_t s2) {
		aencconf.AENC0_offset = (maxVal_0 + minVal_0) / 2;
		aencconf.AENC0_scale = (int16_t)clip<uint32_t, uint32_t>(0xF6FF00 / s0, 1, 32767);
		if(conf.motconf.enctype == EncoderType_TMC::uvw){
			aencconf.AENC1_offset = (maxVal_1 + minVal_1) / 2;
			aencconf.AENC1_scale = (int16_t)clip<uint32_t, uint32_t>(0xF6FF00 / s1, 1, 32767);
		}
		aencconf.AENC2_offset = (maxVal_2 + minVal_2) / 2;
		aencconf.AENC2_scale = (int16_t)clip<uint32_t, uint32_t>(0xF6FF00 / s2, 1, 32767);
	};
	// Enforce a strict 15-second timeout and abort immediately on E-stop or power loss.
	while(stage != 3 && (HAL_GetTick() - aencStartTick < 15000) && canRunMotor()){
		Delay(2);
		if(getPos() > maxpos*poles && stage == 0){
			runOpenLoop(bangInitPower, 0, -speed, 100,true);
			stage = 1;
		}else if(getPos() < minpos*poles && stage == 1){
			// Clamp AENC span to prevent division-by-zero or int16 overflow on low/missing signals.
			uint32_t span0 = std::max<uint32_t>(500, (maxVal_0 > minVal_0) ? (maxVal_0 - minVal_0) : 500);
			uint32_t span1 = std::max<uint32_t>(500, (maxVal_1 > minVal_1) ? (maxVal_1 - minVal_1) : 500);
			uint32_t span2 = std::max<uint32_t>(500, (maxVal_2 > minVal_2) ? (maxVal_2 - minVal_2) : 500);
			applyAencOffsetScale(span0, span1, span2);
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
	if(stage != 3 || !canRunMotor()){
		// Cleanly cut open-loop current if AENC calibration timed out or was interrupted by E-stop.
		rampFlux(0, 50);
		runOpenLoop(0, 0, 0, 1000, true);
		setFluxTorque(0, 0);
		setPhiEtype(lastphie);
		setMotionMode(MotionMode::stop, true);
		setPosSel(possel);
		blinkClipLed(0, 0);
		return;
	}
	// Abort if analog encoder signal amplitude is too low or disconnected.
	uint32_t finalSpan0 = (maxVal_0 > minVal_0) ? (maxVal_0 - minVal_0) : 0;
	uint32_t finalSpan1 = (maxVal_1 > minVal_1) ? (maxVal_1 - minVal_1) : 0;
	uint32_t finalSpan2 = (maxVal_2 > minVal_2) ? (maxVal_2 - minVal_2) : 0;
	if(finalSpan0 < 1000 || finalSpan2 < 1000 || (conf.motconf.enctype == EncoderType_TMC::uvw && finalSpan1 < 1000)){
		setPhiEtype(lastphie);
		setPosSel(possel);
		blinkClipLed(0, 0);
		ErrorHandler::addError(Error(ErrorCode::encoderAlignmentFailed, ErrorType::critical, "Analog encoder signal too low"));
		enterHardError();
		return;
	}
	applyAencOffsetScale(finalSpan0, finalSpan1, finalSpan2);
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
	if((this->conf.motconf.motor_type != MotorType::STEPPER && this->conf.motconf.motor_type != MotorType::BLDC) || (
			conf.motconf.enctype != EncoderType_TMC::uvw && conf.motconf.enctype != EncoderType_TMC::sincos && conf.motconf.enctype != EncoderType_TMC::abn && conf.motconf.enctype != EncoderType_TMC::ext))
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

	auto abortCheck = [&]() {
		setFluxTorque(0, 0);
		setPhiE_ext(0);
		setPhiEtype(lastphie);
		blinkClipLed(0, 0);
		return false;
	};

	//Forward
	int16_t phiE_enc = 0;
	uint16_t failcount = 0;
	int16_t revCount = 0;
	for(int16_t angle = 0;angle<targetAngle && canRunMotor();angle+=0x00ff){
		uint16_t c = 0;
		setPhiE_ext(angle+startAngle);
		Delay(5);
		phiE_enc = getPhiE_Enc() - startAngle;
		int16_t err = abs((int16_t)(phiE_enc - angle));
		int16_t nErr = abs((int16_t)(phiE_enc + angle));
		// Wait more until encoder settles a bit
		while(err > 2000 && nErr > 2000 && c++ < 50 && canRunMotor()){
			phiE_enc = getPhiE_Enc() - startAngle;
			err = abs((int16_t)(phiE_enc - angle));
			nErr = abs((int16_t)(phiE_enc + angle));
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
	if(!canRunMotor()){
		return abortCheck();
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
		enterHardError();
		result = false;
	}

	// Backward

	if(result){ // Only if not already failed
		for(int16_t angle = targetAngle;angle>0 && canRunMotor();angle -= 0x00ff){
			uint16_t c = 0;
			setPhiE_ext(angle+startAngle);
			Delay(5);
			phiE_enc = getPhiE_Enc() - startAngle;
			int16_t err = abs((int16_t)(phiE_enc - angle));
			int16_t nErr = abs((int16_t)(phiE_enc + angle));
			// Wait more
			while(err > 2500 && nErr > 2500 && c++ < 50 && canRunMotor()){
				phiE_enc = getPhiE_Enc() - startAngle;
				err = abs((int16_t)(phiE_enc - angle));
				nErr = abs((int16_t)(phiE_enc + angle));
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
	if(!canRunMotor()){
		return abortCheck();
	}

	// TODO check if we want that
	bool reversedDuringCheck = (revCount > maxcount);
	if(reversedDuringCheck){ // Encoder seems reversed
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

	if(result && reversedDuringCheck){
		// Re-align electrical phase offset after reversing encoder counting direction.
		bangInitEnc(bangInitPower);
		if(!canRunMotor()){
			return abortCheck();
		}
	}

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
	bool allowTemp = conf.hwconf.thermistorSettings.temperatureEnabled;
	conf.hwconf.thermistorSettings.temperatureEnabled = false; // Temp check interrupts adc
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
	while((HAL_GetTick() - tick < measuretime_idle) && !isEmergencyActive() && !brake_failure){ // Measure idle
		writeReg(0x03, 0); // Read raw adc
		uint32_t adcraw = readReg(0x02);
		uint16_t rawA = adcraw & 0xffff;
		uint16_t rawB = (adcraw >> 16) & 0xffff;
		// Signflip filter for SPI bug
		if(abs((int32_t)lastrawA - (int32_t)rawA) < 10000 && abs((int32_t)lastrawB - (int32_t)rawB) < 10000){
			totalA += rawA;
			totalB += rawB;
			measurements_idle++;
			lastrawA = rawA;
			lastrawB = rawB;
		}
	}
	// Guard against zero accepted samples to prevent division-by-zero HardFault.
	if(measurements_idle == 0 || isEmergencyActive() || brake_failure){
		adcCalibrated = false;
		conf.hwconf.thermistorSettings.temperatureEnabled = allowTemp;
		if(!isEmergencyActive() && !brake_failure){
			ErrorHandler::addError(Error(ErrorCode::adcCalibrationError,ErrorType::critical,"TMC ADC offset calibration failed."));
		}
		return false;
	}
	int32_t offsetAidle = totalA / measurements_idle;
	int32_t offsetBidle = totalB / measurements_idle;

	// Check if offsets are in a valid range
	if(totalA < 100 || totalB < 100 || ((abs(offsetAidle - 0x7fff) > TMC_ADCOFFSETFAIL) || (abs(offsetBidle - 0x7fff) > TMC_ADCOFFSETFAIL)) ){
		ErrorHandler::addError(Error(ErrorCode::adcCalibrationError,ErrorType::critical,"TMC ADC offset calibration failed."));
//		blinkErrLed(100, 0); // Blink forever
//		setPwm(TMC_PwmMode::off); //Disable pwm
//		this->changeState(TMC_ControlState::HardError);
		adcCalibrated = false;
		conf.hwconf.thermistorSettings.temperatureEnabled = allowTemp;
		return false; // An adc or shunt amp is likely broken. do not proceed.
	}
	conf.adc_I0_offset = offsetAidle;
	conf.adc_I1_offset = offsetBidle;
	setAdcOffset(conf.adc_I0_offset, conf.adc_I1_offset);
	// ADC Offsets should now be close to perfect

	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);
	adcCalibrated = true;
	conf.hwconf.thermistorSettings.temperatureEnabled = allowTemp;
	return true;
}

void TMC4671::calibFailCb(){
	if(canRunMotor() && calibrationFailCount-- != 0){
		allowStateChange = true;
		changeState(TMC_ControlState::FullCalibration); // retry
	}else{
		fullCalibrationInProgress = false;
		if(isEmergencyActive() || brake_failure){
			enterShutdown();
		}else{
			Error err = Error(ErrorCode::tmcCalibFail,ErrorType::critical,"TMC calibration failed");
			ErrorHandler::addError(err);
			enterHardError();
		}
	}
}
void TMC4671::encoderInit(){

	if(isEmergencyActive() || brake_failure || state == TMC_ControlState::HardError){
		return;
	}
	if(!powerInitialized || !hasPower()){
		this->postPowerState = usingExternalEncoder() ? TMC_ControlState::ExternalEncoderInit : TMC_ControlState::EncoderInit;
		changeState(TMC_ControlState::waitPower, true);
		return;
	}

	allowStateChange = false;
	// Enable power stage safely for alignment even when coming from Shutdown or undervoltage recovery.
	if(!enablePowerStageForCalibration()){
		allowStateChange = true;
		return;
	}

	// Initializes encoder
	if(conf.motconf.enctype == EncoderType_TMC::abn){
		setPosSel(PosSelection::PhiM_abn); // Mechanical Angle
		setVelSel(VelSelection::PhiM_abn); // Mechanical Angle (RPM)
		if(!encHallRestored){
			estimateABNparams(); // If not saved try to estimate parameters
			recalibrationRequired = true;
		}
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		setPosSel(PosSelection::PhiM_aenc); // Mechanical Angle
		setVelSel(VelSelection::PhiM_aenc); // Mechanical Angle (RPM)
		if(!conf.hwconf.flags.analog_enc_skip_cal){
			calibrateAenc();
		}
	}

	if(!canRunMotor()){
		if(state != TMC_ControlState::HardError) allowStateChange = true;
		return;
	}

	// find index
	if(conf.motconf.enctype == EncoderType_TMC::abn && abnconf.useIndex && !encoderIndexHitFlag){ // TODO changing direction might invalidate phiE offset because of index pulse width
		findEncoderIndex(abnconf.posOffsetFromIndex < 0 ? 10 : -10,bangInitPower/2,true,true); // Go to index and zero encoder
		setPhiEtype(PhiE::openloop); // Openloop used in last step. Use for aligning too
	}else{
		setPhiE_ext(getPhiE());
		setPhiEtype(PhiE::ext);
	}

	if(!canRunMotor()){
		if(state != TMC_ControlState::HardError) allowStateChange = true;
		return;
	}

	// Align encoder
	bangInitEnc(bangInitPower);

	if(!canRunMotor()){
		if(state != TMC_ControlState::HardError) allowStateChange = true;
		return;
	}

	// Check encoder
	if(!checkEncoder()){
		// Only count alignment retries on actual encoder failures, never on E-stop or power loss.
		if(canRunMotor() && ++enc_retry > ENC_RETRY_MAX){
			encoderAligned = false;
			Error err = Error(ErrorCode::encoderAlignmentFailed,ErrorType::critical,"Encoder alignment failed");
			ErrorHandler::addError(err);
			enterHardError();
		}

		if(manualEncAlign){
			manualEncAlign = false;
			CommandHandler::broadcastCommandReply(CommandReply("Error during check",1), (uint32_t)TMC4671_commands::encalign, CMDtype::get);
		}
		if(state != TMC_ControlState::HardError) allowStateChange = true;
		return;
	}
	enc_retry = 0;
	encoderAligned = true;

	if(conf.motconf.enctype == EncoderType_TMC::abn && abnconf.useIndex && encoderIndexHitFlag)
		setTmcPos(getPosAbs() - abnconf.posOffsetFromIndex); // Load stored position

	if(conf.motconf.enctype == EncoderType_TMC::abn){
		setPhiEtype(PhiE::abn);
	}else if(conf.motconf.enctype == EncoderType_TMC::sincos || conf.motconf.enctype == EncoderType_TMC::uvw){
		setPhiEtype(PhiE::aenc);
	}else if(usingExternalEncoder()){
		setPhiEtype(PhiE::extEncoder);
	}

	if(manualEncAlign){
		manualEncAlign = false;
		CommandHandler::broadcastCommandReply(CommandReply("Aligned successfully",1), (uint32_t)TMC4671_commands::encalign, CMDtype::get);
	}
	allowStateChange = true;
	changeState(TMC_ControlState::EncoderFinished);
}

/**
 * Changes the encoder type and calls init methods for the encoder types.
 * Setup the specific parameters (abnconf, aencconf...) first.
 */
void TMC4671::setEncoderType(EncoderType_TMC type){
	// If no external timer is set external encoder is not valid
	if( !conf.hwconf.isEncSupported(type) || ((!externalEncoderTimer || !externalEncoderAllowed()) && type == EncoderType_TMC::ext)){
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
	}else if(type == EncoderType_TMC::ext){
		// Keep encoderAligned false while waiting for Axis to attach the external encoder instance.
		encoderAligned = false;
		changeState(TMC_ControlState::Shutdown);
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
	// Block open-loop excitation when emergency stop is active unless zeroing output.
	if(isEmergencyActive() && (ud != 0 || uq != 0 || speed != 0)){
		setFluxTorque(0, 0);
		setOpenLoopSpeedAccel(0, accel);
		return;
	}
	if(this->conf.motconf.motor_type == MotorType::DC){
		uq = ud+uq; // dc motor has no flux. add to torque
	}
	if(ud != 0 || uq != 0 || speed != 0){
		if(encoderAligned){
			startMotor();
		}else if(!enablePowerStageForCalibration()){
			return;
		}
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
	int16_t oldPhiE = getPhiE();
	setPhiEtype(PhiE::openloop);
	writeReg(0x23,oldPhiE); // Start running at last phiE value

	setOpenLoopSpeedAccel(speed, accel);
}

void TMC4671::setUdUq(int16_t ud,int16_t uq){
	if(isEmergencyActive()){
		ud = 0;
		uq = 0;
	}
	writeReg(0x24, (ud & 0xffff) | ((uint32_t)(uint16_t)uq << 16));
}

bool TMC4671::canRunMotor(){
	// Require no emergency stop, no brake resistor fault, no latched HardError, and valid bus power.
	return !isEmergencyActive() && !brake_failure && state != TMC_ControlState::HardError && hasPower();
}

bool TMC4671::enablePowerStageForCalibration(bool requireAdcCalib){
	// Safely energize FOC PWM and DRV_ENABLE for privileged alignment/calibration routines.
	if(!initialized || !canRunMotor() || conf.motconf.motor_type == MotorType::NONE){
		return false;
	}
	if(requireAdcCalib && (!powerInitialized || !adcCalibrated)){
		return false;
	}
	setFluxTorque(0, 0);
	setPwm(TMC_PwmMode::PWM_FOC);
	enablePin.set();
	return true;
}

void TMC4671::disablePowerStage(bool resetEnablePin){
	// Disable driver enable pin (optional) and zero hardware torque, motion mode, and FOC PWM.
	if(resetEnablePin){
		enablePin.reset();
	}
	if(initialized && !inIsr()){
		setFluxTorque(0, 0);
		setMotionMode(MotionMode::stop, true);
		setPwm(TMC_PwmMode::off);
	}
}

void TMC4671::enterShutdown(){
	// Cut power stage and force both active and requested states to Shutdown.
	motorEnabledRequested = false;
	disablePowerStage(true);
	allowStateChange = true;
	changeState(TMC_ControlState::Shutdown, false);
	changeState(TMC_ControlState::Shutdown, true);
}

void TMC4671::enterWaitPower(){
	// Cut power stage, set a clean post-power target state, and force privileged waitPower.
	disablePowerStage(true);
	TMC_ControlState resumeState = (motorEnabledRequested && isSetUp()) ? TMC_ControlState::Running : TMC_ControlState::Shutdown;
	changeState(resumeState, false);
	changeState(TMC_ControlState::waitPower, true);
}

void TMC4671::enterHardError(){
	// Cut power stage, flag emergency, and permanently latch HardError across all state variables.
	emergency = true;
	motorEnabledRequested = false;
	fullCalibrationInProgress = false;
	disablePowerStage(true);
	changeState(TMC_ControlState::HardError, true);
}

bool TMC4671::isHardwareEnableHigh(){
	// Read TMC4671 ENABLE_IN bit (0x76 bit 15) while rejecting invalid SPI bus readings.
	uint32_t pins = readReg(0x76);
	return ((pins >> 15) & 0x01) && pins != 0xffffffff && pins != 0;
}

bool TMC4671::checkHardwareEstop(){
	// Detect external E-stop pulling TMC ENABLE_IN low while MCU drives DRV_ENABLE high.
	if(HAL_GPIO_ReadPin(DRV_ENABLE_GPIO_Port, DRV_ENABLE_Pin) == GPIO_PIN_SET && !isHardwareEnableHigh()){
		this->estopTriggered = true;
		this->emergencyStop(false);
		return true;
	}
	return false;
}

void TMC4671::stopMotor(){
	// Always cut torque, motion mode, and FOC PWM regardless of current state.
	motorEnabledRequested = false;
	disablePowerStage(false);
	if(state == TMC_ControlState::Running || state == TMC_ControlState::EncoderFinished){
		changeState(TMC_ControlState::Shutdown);
	}
}
void TMC4671::startMotor(){
	motorEnabledRequested = true;
	if(isEmergencyActive() || brake_failure || state == TMC_ControlState::HardError || conf.motconf.motor_type == MotorType::NONE){
		return;
	}

	if(state == TMC_ControlState::Shutdown){
		if(!initialized){
			changeState(TMC_ControlState::uninitialized, true);
			return;
		}
		if(!powerInitialized || !hasPower()){
			changeState(TMC_ControlState::waitPower, true);
			return;
		}
		if(!encoderAligned){
			// Trigger encoder alignment before entering Running state.
			setEncoderType(conf.motconf.enctype);
			return;
		}
		changeState(TMC_ControlState::Running);
	}
	// Only enable power stage when fully initialized, aligned, powered, and no fault is active.
	if(initialized && powerInitialized && encoderAligned && canRunMotor()){
		setFluxTorque(0, 0);
		setPwm(TMC_PwmMode::PWM_FOC); // enable foc
		enablePin.set();
		setMotionMode(nextMotionMode,true);
	}
}

void TMC4671::emergencyStop(bool reset){
	if(!reset){
		emergency = true;
		enablePin.reset(); // Cut hardware driver enable pin immediately (ISR-safe).
		motorEnabledRequested = false;
		// Defer blocking SPI shutdown commands to the TMC thread when called from an ISR.
		if(!inIsr()){
			this->stopMotor();
		}
	}else{
		emergency = false;
		motorEnabledRequested = true;
		if(!inIsr()){
			this->startMotor();
			if(!motorReady()){
				Resume();
			}
		}else{
			ResumeFromISR();
		}
	}
}

/**
 * Calculates a flux value based on the internal and external voltage difference to dissipate energy without
 * a brake resistor
 */
int16_t TMC4671::controlFluxDissipate(){

	int32_t vDiff = getIntV() - getExtV();
	if(vDiff > FLUX_DISSIPATION_LIMIT){
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
	// Avoid blocking SPI transactions inside an ISR; emergencyStop handles hardware pin cutoff.
	if(inIsr() || !(this->motorReady() && motorEnabledRequested))
		return;

	int32_t flux = 0;
	// Apply Axis-level force inversion only to incoming game torque, not to internal anticogging torque.
	int32_t totalPower = conf.invertForce ? -(int32_t)power : (int32_t)power;

	// Anticogging & dynamic speed-dependent ripple compensation
#ifdef COGGING_TABLE_FLASH_START_ADDRESS
	if (cogging_enabled && !this->isCalibrationInProgress()) {
		float pos_f = this->getFilteredPosition();

		// 1. Ultra-fast instantaneous speed estimation (turns/s) from pos_f (0 extra SPI reads)
		uint32_t now_us = micros();
		uint32_t dt_us = now_us - last_turn_us;
		last_turn_us = now_us;
		if (dt_us > 50 && dt_us < 50000) {
			float dpos = getWrappedError(pos_f, last_turn_pos_f);
			float inst_vel = (dpos * 1000000.0f) / (float)dt_us;
			turn_vel_filtered += 0.25f * (inst_vel - turn_vel_filtered);
		} else {
			turn_vel_filtered = 0.0f;
		}
		last_turn_pos_f = pos_f;

		float abs_vel = fabsf(turn_vel_filtered);

		// 2. Bounded RL/PI asymptotic speed factor S(w) = |w| / (|w| + w_c) in [0.0 .. 1.0] computed ONCE outside loop
		float speed_factor = abs_vel / (abs_vel + COGGING_RIPPLE_CORNER_VEL);

		// 3. Predictive phase-lead factored OUTSIDE the harmonic loop (0 extra trig cost inside loop)
		// Automatically applies delta_phi_k = 2*PI * k * w * tau_lead to every harmonic order k
		float pred_pos_f = pos_f + (turn_vel_filtered * this->cogging_phase_lead_time);
		float angle_rad = pred_pos_f * (2.0f * PI);

		// 4. Fourier synthesis with speed-dependent ripple modulation and ZOH anti-aliasing roll-off
		float compensation = 0.0f;
		for (uint8_t i = 0; i < COGGING_HARMONICS_COUNT; i++) {
			float base_amp = cogging_harmonics[i].amplitude;
			if (base_amp > 0.0f) {
				uint16_t order = cogging_harmonics[i].order;
				float harm_freq_hz = (float)order * abs_vel;

				// Soft anti-aliasing cut-off above 200..350 Hz to prevent ZOH phase inversion at high RPM
				if (harm_freq_hz < 350.0f) {
					float dyn_amp = base_amp + ((float)cogging_harmonics[i].speed_delta * speed_factor);
					dyn_amp = clip<float, float>(dyn_amp, 0.0f, base_amp * 3.0f);
					if (harm_freq_hz > 200.0f) {
						dyn_amp *= (350.0f - harm_freq_hz) * (1.0f / 150.0f);
					}
					compensation += dyn_amp * arm_sin_f32(angle_rad * (float)order + cogging_harmonics[i].phase);
				}
			}
		}
		float scaled_comp = this->cogging_scale * compensation;
		float comp_limit = (curLimits.pid_torque_flux > 0) ? (float)curLimits.pid_torque_flux : 30000.0f;
		this->last_anticogging_torque = (int32_t)clip<float, float>(scaled_comp, -comp_limit, comp_limit);

		totalPower += this->last_anticogging_torque;
	} else {
		this->last_anticogging_torque = 0;
	}
#endif

	// Invert total motor torque only when commutating via a reversed external encoder.
	if(this->conf.encoderReversed && conf.motconf.enctype == EncoderType_TMC::ext){
		totalPower = -totalPower;
	}

	// Flux offset for field weakening
	flux = idleFlux-clip<int32_t,int16_t>(abs(power),0,maxOffsetFlux);

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

	setFluxTorque(flux, clip<int32_t, int16_t>(totalPower, -32768, 32767));
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

	// Use bitwise mask to extract 16-bit major and minor chip version numbers.
	std::string versionstring = std::to_string((versionInt >> 16) & 0xffff) + "." + std::to_string((versionInt) & 0xffff);

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
		// Force re-alignment whenever an external encoder instance is attached.
		encoderAligned = false;
		if(!extEncUpdater){ // If updater has not been set up because the encoder mode was changed before the external encoder passed force it now
			setUpExtEncTimer();
		}
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
	this->cached_pos = pos;
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
	return 0x10000; // TMC4671 internal 16-bit PhiM / pos register (0x6B) wraps at 65536 counts per revolution
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
	// Use 64-bit scaling and guard against zero CPR to prevent division-by-zero on high-res ABN encoders.
	if(abnconf.cpr == 0) return 0;
	return (uint32_t)(((uint64_t)enc * 0xffff) / abnconf.cpr);
}
uint32_t TMC4671::posToEnc(uint32_t pos){
	// Use 64-bit scaling so CPR > 65535 does not truncate (0xffff / cpr) to zero before division.
	if(abnconf.cpr == 0) return 0;
	return (uint32_t)((((uint64_t)pos * abnconf.cpr) / 0xffff) % abnconf.cpr);
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
	return allowExternalEncoder && conf.hwconf.flags.enc_ext;
#endif
}

void TMC4671::setMotorType(MotorType motor,uint16_t poles){

	if(!conf.hwconf.isMotSupported(motor)){
		motor = MotorType::NONE;
	}
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
	if(isEmergencyActive()){
		torque = 0;
	}else if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque,true);
	}
	torque = clampTorqueFlux(torque);

	// Update main torque setpoint
	updateReg(0x64, (uint16_t)torque, 0xffff, 16);
}

int16_t TMC4671::getTorque(){
	return readReg(0x64) >> 16;
}

void TMC4671::setFlux(int16_t flux){
	if(isEmergencyActive()){
		flux = 0;
	}else if(curMotionMode != MotionMode::torque){
		setMotionMode(MotionMode::torque,true);
	}
	flux = clampTorqueFlux(flux);
	updateReg(0x64,(uint16_t)flux,0xffff,0);
}
int16_t TMC4671::getFlux(){
	// Use bitwise AND to return signed 16-bit flux setpoint from register 0x64.
	return (int16_t)(readReg(0x64) & 0xffff);
}
void TMC4671::setFluxTorque(int16_t flux, int16_t torque){
	// Force zero flux and torque when emergency stop is active while still writing 0 to hardware.
	if(isEmergencyActive()){
		flux = 0;
		torque = 0;
	}else{
		flux = clampTorqueFlux(flux);
		torque = clampTorqueFlux(torque);
		if(curMotionMode != MotionMode::torque){
			setMotionMode(MotionMode::torque,true);
		}
	}
	if(inIsr()){
		return;
	}
	uint32_t packed = ((uint16_t)flux & 0xffff) | ((uint32_t)(uint16_t)torque << 16);
#ifdef TMC4671_TORQUE_USE_ASYNC
	writeRegAsync(0x64, packed);
#else
	// Update main flux and torque setpoints
	writeReg(0x64, packed);
#endif
}


void TMC4671::setFluxTorqueFF(int16_t flux, int16_t torque){
	if(isEmergencyActive()){
		flux = 0;
		torque = 0;
	}else{
		flux = clampTorqueFlux(flux);
		torque = clampTorqueFlux(torque);
		if(curMotionMode != MotionMode::torque){
			setMotionMode(MotionMode::torque,true);
		}
	}
	writeReg(0x65, ((uint16_t)flux & 0xffff) | ((uint32_t)(uint16_t)torque << 16));
}

/**
 * Ramps flux from current value to a target value over a specified duration
 */
void TMC4671::rampFlux(uint16_t target,uint16_t time_ms){
	if(!canRunMotor()){
		setFluxTorque(0, 0);
		return;
	}
	uint16_t startFlux = readReg(0x64) & 0xffff;
	int32_t stepsize = (target - startFlux) / std::max<uint16_t>(1, time_ms/2);
	if(stepsize == 0){
		stepsize = startFlux < target ? 1 : -1;
	}
	uint16_t flux = startFlux;
	while(abs(target - flux) >= abs(stepsize)){
		if(!canRunMotor()){
			// Cut flux immediately if emergency stop or power loss occurs during ramp.
			setFluxTorque(0, 0);
			return;
		}
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

void TMC4671::setPowerLimit(uint16_t power) {
	maxPowerAxis = power;
	setTorqueLimit(power);
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
 *  Set both 0xffff to deactivate
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
	for(int16_t p = 0;p<0x0fff && canRunMotor();p+=0x2f){
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
	if(!canRunMotor() || c == 0){
		setFluxTorque(0, 0);
		blinkClipLed(0, 0);
		return;
	}

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
	for(int16_t p = 0;p<0x0fff && canRunMotor();p+=0x2f){
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
	if(!canRunMotor() || c == 0){
		setFluxTorque(0, 0);
		blinkClipLed(0, 0);
		return;
	}

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
	return std::make_pair((int32_t)af, (int32_t)at);
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
		// Defer SPI status register reads from ISR to the TMC thread to prevent SPI bus corruption.
		flagCheckPending = true;
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

	setHwType((uint8_t)((val >> 11) & 0x1F));

}



/**
 * Sets some constants and features depending on the hardware version of the driver
 */
void TMC4671::setHwType(uint8_t type){
	// If only one config is valid use this regardless of requested type
	if(TMC4671::tmc4671_hw_configs.size() == 1){
		this->conf.hwconf = TMC4671::tmc4671_hw_configs[0];
	}else{ // Search for config matching requested type
		for(const TMC4671HardwareTypeConf& newConf : TMC4671::tmc4671_hw_configs){
			if(type == newConf.hwVersion){
				this->conf.hwconf = newConf;
				break;
			}
		}
	}

	setVSenseMult(this->conf.hwconf.vSenseMult); // Update vsense multiplier
	//setupBrakePin(vdiffAct, vdiffDeact, vMax); // TODO if required
	setBrakeLimits(this->conf.hwconf.brakeLimLow,this->conf.hwconf.brakeLimHigh);
	setBBM(this->conf.hwconf.bbm,this->conf.hwconf.bbm);
	// Force changing motor and encoder types to prevent invalid types being selected if new hw type does not support them
	setMotorType(this->conf.motconf.motor_type, this->conf.motconf.pole_pairs);
	setEncoderType(this->conf.motconf.enctype);
}

/**
 * Appends a formatted reply with currently available hardware version configs
 */
void TMC4671::replyHardwareVersions(const std::span<const TMC4671HardwareTypeConf>& versions,std::vector<CommandReply>& replies){
//	uint8_t idx = 0;
	for(const TMC4671HardwareTypeConf& c : versions){
		if(this->canChangeHwType || c.hwVersion == this->conf.hwconf.hwVersion){
			replies.emplace_back( std::to_string((uint8_t)c.hwVersion) + ":" + c.name,(uint8_t)c.hwVersion);
		}
	}
}

void TMC4671::registerCommands(){
	CommandHandler::registerCommands();

	registerCommand("cpr", TMC4671_commands::cpr, "CPR in TMC",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("mtype", TMC4671_commands::mtype, "Motor type",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("encsrc", TMC4671_commands::encsrc, "Encoder source",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("tmcHwType", TMC4671_commands::tmcHwType, "Version of TMC board",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("encalign", TMC4671_commands::encalign, "Align encoder",CMDFLAG_GET);
	registerCommand("poles", TMC4671_commands::poles, "Motor pole pairs",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("acttrq", TMC4671_commands::acttrq, "Measure torque, flux, cogging",CMDFLAG_GET);
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
#ifdef COGGING_TABLE_FLASH_START_ADDRESS
	registerCommand("cogging", TMC4671_commands::cogging, "Get/Set the cogging compensation",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("calibrateCogging", TMC4671_commands::calibrateCogging, "Cogging calibration",CMDFLAG_GET);
	registerCommand("coggingTable", TMC4671_commands::coggingTable, "Get the cogging table, or clear table (set 0)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("coggingScale", TMC4671_commands::coggingScale, "Cogging compensation scale (0-100)",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("coggingSpeedP", TMC4671_commands::coggingSpeedP, "Manual speed loop P gain for cogging",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("coggingSpeedI", TMC4671_commands::coggingSpeedI, "Manual speed loop I gain for cogging",CMDFLAG_GET | CMDFLAG_SET);
#endif
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
		}else if(cmd.type == CMDtype::set && (uint8_t)cmd.val <= (uint8_t)MotorType::BLDC){
			this->setMotorType((MotorType)cmd.val, this->conf.motconf.pole_pairs);
		}else{
			std::string rplstr = "";
			TMC4671HardwareTypeConf::SupportedModes_s* confflags = &conf.hwconf.flags;
			if(confflags->mot_none) rplstr += "NONE=0,";
			if(confflags->mot_dc) rplstr += "DC=1,";
			if(confflags->mot_stepper) rplstr += "Stepper 2Ph=2,";
			if(confflags->mot_bldc) rplstr += "BLDC 3Ph=3";
			replies.emplace_back(rplstr);
		}
		break;

	case TMC4671_commands::encsrc:
		if(cmd.type == CMDtype::get){
			replies.emplace_back((uint8_t)this->conf.motconf.enctype);
		}else if(cmd.type == CMDtype::set){
			this->setEncoderType((EncoderType_TMC)cmd.val);
		}else{
			std::string rplstr = "";
			TMC4671HardwareTypeConf::SupportedModes_s* confflags = &conf.hwconf.flags;
			if(confflags->enc_none) rplstr += "NONE=0,";
			if(confflags->enc_abn) rplstr += "ABN=1,";
			if(confflags->enc_sincos) rplstr += "SinCos=2,";
			if(confflags->enc_uvw) rplstr += "UVW=3,";
			if(confflags->enc_hall) rplstr += "HALL=4,";
			if(confflags->enc_ext && externalEncoderAllowed()) rplstr += "External=5";
			replies.emplace_back(rplstr);
		}
		break;

	case TMC4671_commands::tmcHwType:
		if(cmd.type == CMDtype::get){
			replies.push_back((uint8_t)conf.hwconf.hwVersion);
		}else if(cmd.type == CMDtype::set){
			if(canChangeHwType)
				setHwType((uint8_t)(cmd.val & 0x1F));
		}else{
			// List known hardware versions
			replyHardwareVersions(tmc4671_hw_configs, replies);
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
			std::string replyStr = std::to_string(current.second) + ":" + std::to_string(current.first);
#ifdef COGGING_TABLE_FLASH_START_ADDRESS
			replyStr += ":" + std::to_string(this->last_anticogging_torque);
#endif
			replies.emplace_back(CommandReply(replyStr, current.second, current.first));
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

#ifdef COGGING_TABLE_FLASH_START_ADDRESS
	case TMC4671_commands::cogging:
		handleGetSet(cmd, replies, cogging_enabled);
		break;
	
	case TMC4671_commands::calibrateCogging:

		if (isEmergencyActive() && hasPower()) {
			replies.emplace_back("Calibration aborted: Emergency stop engaged", 0);
		} else if (conf.motconf.motor_type == MotorType::NONE) {
			replies.emplace_back("Calibration aborted: No motor type set", 0);
		} else if (hasPower() && (!encoderAligned || !adcCalibrated || conf.motconf.enctype == EncoderType_TMC::NONE)) {
			// Prevent powered cogging calibration if encoder or ADC is not yet calibrated and aligned.
			replies.emplace_back("Calibration aborted: Encoder or ADC not aligned", 0);
		} else {
			changeState(TMC_ControlState::CoggingCalibration);
			return CommandStatus::NO_REPLY;
		}

		break;


	case TMC4671_commands::coggingTable:
		if(cmd.type == CMDtype::get){
			std::string s;
			s.reserve(512);
			s = "item:0,data:(";

			for (uint16_t order = 0; order < COGGING_CALIB_DFT_HARMONICS; order++) {
				
				int16_t val = 0;
				for (uint8_t n = 0; n < COGGING_HARMONICS_COUNT; n++) {
					if (cogging_harmonics[n].order == order && cogging_harmonics[n].amplitude > 0.0f) {
						val = (int16_t)cogging_harmonics[n].amplitude;
						break;
					}
				}

				s.append(std::to_string(val));

				if (s.length() >= 500) { // send data by string cut at 500 items
					s.append(")");
					CommandHandler::broadcastCommandReply(CommandReply(s,0), (uint32_t)TMC4671_commands::coggingTable, CMDtype::get);
					s.clear();
					s = "item:" + std::to_string(order+1) + ",data:(";
				} else {
					if (order < COGGING_CALIB_DFT_HARMONICS - 1) {
						s.append(",");
					}
				}
			}

			s.append(")");
			CommandHandler::broadcastCommandReply(CommandReply(s,0), (uint32_t)TMC4671_commands::coggingTable, CMDtype::get);
			return CommandStatus::NO_REPLY;
		} else if(cmd.type == CMDtype::set && cmd.val == 0) {
			clearCoggingTable();
		} else {
			status = CommandStatus::ERR;
		}
		break;

	case TMC4671_commands::coggingScale:
		if (cmd.type == CMDtype::get) {
			replies.emplace_back((int16_t)(this->cogging_scale * 10000.0f));
		} else if (cmd.type == CMDtype::set) {
			this->cogging_scale = (float)(int16_t)cmd.val / 10000.0f;
		}
		break;

	case TMC4671_commands::coggingSpeedP:
		handleGetSet(cmd, replies, this->coggingSpeedP);
		break;

	case TMC4671_commands::coggingSpeedI:
		handleGetSet(cmd, replies, this->coggingSpeedI);
		break;
#endif

		default:
			return CommandStatus::NOT_FOUND;
	}
	return status;


}

#if defined(TIM_TMC) || defined(TIM_CALIBRATION)
	void TMC4671::timerElapsed(TIM_HandleTypeDef* htim){
#ifdef TIM_CALIBRATION
		// If calibration timer triggered the interrupt, notify the waiting calibration loop thread.
		if(this->calibTimer != nullptr && htim == this->calibTimer){
			this->NotifyFromISR();
			return;
		}
#endif
#ifdef TIM_TMC
		if(htim == this->externalEncoderTimer){
			// Read encoder and send to tmc
			if(usingExternalEncoder() && externalEncoderAllowed() && this->conf.motconf.phiEsource == PhiE::extEncoder && extEncUpdater != nullptr){
				//setPhiE_ext(getPhiEfromExternalEncoder());
				// Signal phiE update
				extEncUpdater->updateFromIsr(); // Use task so that the update is not being done inside an ISR
			}

			// If we are using the external encoder timer to pace calibration
			if (usingExternalEncoder() && this->calibTicksTarget > 0) {
				this->calibTicksCount = this->calibTicksCount + 1;
				if (this->calibTicksCount >= this->calibTicksTarget) {
					this->calibTicksCount = 0;
					this->NotifyFromISR();
				}
			}
		}
#endif
	}
#endif

void TMC4671::setUpExtEncTimer(){
#ifdef TIM_TMC
	if(extEncUpdater == nullptr) // Create updater thread
		extEncUpdater = std::make_unique<TMC_ExternalEncoderUpdateThread>(this);
	// Setup timer
	this->externalEncoderTimer = &TIM_TMC;
	this->externalEncoderTimer->Instance->ARR = TIM_TMC_ARR; // 200 = 5khz = 5 tmc cycles, 250 = 4khz, 240 = 6 tmc cycles
	this->externalEncoderTimer->Instance->PSC = ((TIM_TMC_BCLK)/1000000) +1; // 1µs ticks
	this->externalEncoderTimer->Instance->CR1 = 1;
	HAL_TIM_Base_Start_IT(this->externalEncoderTimer);
#endif
}

/**
 * Configures and starts the calibration hardware timer, or sets up tick counting
 * using the external encoder timer if in use, to avoid desynchronization.
 */
void TMC4671::startCalibTimers(uint32_t period_us) {
#ifdef TIM_TMC
	// If an external encoder is used, we pace the calibration using the existing encoder timer
	// to avoid stopping, reconfiguring, or desynchronizing it.
	if (usingExternalEncoder() && this->externalEncoderTimer != nullptr) {
		uint32_t tmc_period = this->externalEncoderTimer->Instance->ARR;
		if (tmc_period == 0) {
			tmc_period = TIM_TMC_ARR;
		}
		this->calibTicksTarget = period_us / tmc_period;
		if (this->calibTicksTarget == 0) {
			this->calibTicksTarget = 1;
		}
		this->calibTicksCount = 0;
	} else
#endif
	{
		this->calibTicksTarget = 0;
		this->calibTicksCount = 0;
#ifdef TIM_CALIBRATION
		if (this->calibTimer != nullptr) {
			// Stop the timer before changing configurations
			HAL_TIM_Base_Stop_IT(this->calibTimer);
			// Configure the prescaler to count in microseconds (1 MHz frequency)
			this->calibTimer->Instance->PSC = (SystemCoreClock / 1000000) - 1;
			// Auto-reload register defines the period in microseconds
			this->calibTimer->Instance->ARR = period_us;
			// Pre-charge counter to trigger the first interrupt quickly
			this->calibTimer->Instance->CNT = period_us - 20;
			this->calibTimer->Instance->CR1 = 1;
			HAL_TIM_Base_Start_IT(this->calibTimer);
		}
#endif
	}
	// Clear any pending thread notifications before starting wait loops
	this->WaitForNotification(0);
}

/**
 * Stops the calibration hardware timer and resets the calibration tick target.
 */
void TMC4671::stopCalibTimers() {
	this->calibTicksTarget = 0;
	this->calibTicksCount = 0;
#ifdef TIM_CALIBRATION
	if (this->calibTimer != nullptr) {
		HAL_TIM_Base_Stop_IT(this->calibTimer);
	}
#endif
}

/**
 * Returns the actual calibration period in microseconds based on the active timer pacing source.
 */
uint32_t TMC4671::getActualCalibPeriod(uint32_t target_period_us) {
#ifdef TIM_TMC
	// If pacing calibration using TIM_TMC, the actual period is a multiple of TIM_TMC's ARR period.
	if (usingExternalEncoder() && this->externalEncoderTimer != nullptr) {
		uint32_t tmc_period = this->externalEncoderTimer->Instance->ARR;
		if (tmc_period == 0) {
			tmc_period = TIM_TMC_ARR;
		}
		uint32_t ticks = target_period_us / tmc_period;
		if (ticks == 0) {
			ticks = 1;
		}
		return tmc_period * ticks;
	}
#endif
	return target_period_us;
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
		// Re-verify PhiE source before writing 0x1C so calibration open-loop/fixed phase is never overwritten.
		if(tmc->usingExternalEncoder() && tmc->externalEncoderAllowed() && tmc->conf.motconf.phiEsource == PhiE::extEncoder && !tmc->spiPort.isTaken()){
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
		// Shut down power stage and permanently latch HardError.
		emergencyStop(false);
		enterHardError();
	}
}

uint16_t TMC4671::getDrvSlewRate(){
	return this->maxSlewRate;
}

bool TMC4671::startSlewRateCalibration(){
	// Request the TMC internal state machine to run the slew rate calibration.
	// changeState will schedule the calibration state in the driver's thread.
	this->changeState(TMC_ControlState::SlewRateCalibration);
	return true;
}

bool TMC4671::isSlewRateCalibrationInProgress(){
	return (this->state == TMC_ControlState::SlewRateCalibration);
}

bool TMC4671::isCalibrationInProgress() {
	return (state == TMC_ControlState::FullCalibration || 
#ifdef COGGING_TABLE_FLASH_START_ADDRESS
			state == TMC_ControlState::CoggingCalibration || 
#endif
			state == TMC_ControlState::Pidautotune || 
			state == TMC_ControlState::IndexSearch ||
			state == TMC_ControlState::EncoderInit ||
			state == TMC_ControlState::ExternalEncoderInit ||
			state == TMC_ControlState::SlewRateCalibration);
}

void TMC4671::measureMaxSlewRate(){
	if(!enablePowerStageForCalibration()){
		return;
	}
	MotionMode lastmode = getMotionMode();
	PhiE lastphie = getPhiEtype();

	// Setup
	setPhiE_ext(getPhiE());
	setPhiEtype(PhiE::ext); // Fixed phase
	setMotionMode(MotionMode::torque, true);
	setFluxTorque(0, 0);

	int32_t start_flux = getActualFlux();
	uint32_t start_time = micros();
	setFluxTorque(maxPowerAxis,0); // Apply max flux

	uint32_t elapsed_time = 0;
	int32_t max_slew_rate = 0;

	while(elapsed_time < 3000 && canRunMotor()){ // measure for 3ms
		int32_t current_flux = getActualFlux();
		uint32_t current_time = micros();
		elapsed_time = current_time - start_time;

		if(elapsed_time > 0){
			int32_t slew_rate = (current_flux - start_flux) * 1000 / elapsed_time; // Pn/ms (Power normalized/ms)
			if(slew_rate > max_slew_rate){
				max_slew_rate = slew_rate;
			}
		}
	}
	setFluxTorque(0,0);
	if(canRunMotor()){
		this->maxSlewRate = clip<int32_t>(max_slew_rate, 0, MAX_SLEW_RATE);
	}

	// Restore
	setPhiEtype(lastphie);
	setMotionMode(lastmode,true);
}

void TMC4671::handleStateWaitPower() {
	allowStateChange = false;
	pulseClipLed(); // blink led

	if (!hasPower() || isEmergencyActive() || brake_failure) {
		this->powerCheckCounter = 0;
		// Special case: Allow CoggingCalibration simulation even without power when not in emergency.
		if (!isEmergencyActive() && !brake_failure && requestedState == TMC_ControlState::CoggingCalibration) {
			allowStateChange = true;
		}
		Delay(250);
		return;
	}

	if (++this->powerCheckCounter > 5) {
		if (!powerInitialized) {
			if (!initializeWithPower()) {
				// Keep waiting in waitPower when ADC calibration fails instead of leaking state transitions.
				this->powerCheckCounter = 0;
				Delay(100);
				return;
			}
		} else if (readReg(0x0A) == 0 || (conf.motconf.motor_type != MotorType::NONE && (readReg(0x1B) & 0xff0000) == 0)) {
			// Detect hardware brownout reset of TMC4671 registers and force full re-initialization.
			initialized = false;
			powerInitialized = false;
			encoderAligned = false;
			changeState(TMC_ControlState::uninitialized, true);
			return;
		} else {
			ErrorHandler::clearError(ErrorCode::undervoltage);
			if (encoderAligned && motorEnabledRequested && canRunMotor()) {
				// Safely restore power stage after a temporary undervoltage or overvoltage recovery.
				setFluxTorque(0, 0);
				enablePin.set();
				setPwm(TMC_PwmMode::PWM_FOC);
				setMotionMode(nextMotionMode, true);
			}
		}
		allowStateChange = true;

		// If a calibration was pending power, go there now
		if (this->postPowerState != TMC_ControlState::NONE) {
			changeState(this->postPowerState);
			this->postPowerState = TMC_ControlState::NONE;
		} else if (encoderAligned) {
			// Normal flow if encoder is already aligned
			changeState(requestedState);
		}
	}
	Delay(100);
}

void TMC4671::handleStateRunning() {

	// Check status, Temps, Everything alright?
	uint32_t tick = HAL_GetTick();
	if (tick - lastStatTime > 200) { // Every 200ms for fast fault detection
		lastStatTime = tick;
		statusCheck();
		if (motorEnabledRequested && checkHardwareEstop()) {
			ErrorHandler::addError(ESTOP_ERROR);
		}

		// Temperature sense
		if (conf.hwconf.thermistorSettings.temperatureEnabled) {
			float temp = getTemp();
			if (temp > conf.hwconf.thermistorSettings.temp_limit) {
				changeState(TMC_ControlState::OverTemp);
				pulseErrLed();
			}
		}
	}
}

void TMC4671::handleStateFullCalibration() {
	if (isEmergencyActive() || brake_failure || state == TMC_ControlState::HardError) {
		fullCalibrationInProgress = false;
		if (state != TMC_ControlState::HardError) {
			enterShutdown();
		}
		return;
	}
	if (conf.motconf.motor_type == MotorType::NONE) {
		fullCalibrationInProgress = false;
		changeState(TMC_ControlState::Shutdown);
		return;
	}
	if (!hasPower()) {
		this->postPowerState = TMC_ControlState::FullCalibration;
		changeState(TMC_ControlState::waitPower);
		return;
	}

	allowStateChange = false;
	fullCalibrationInProgress = true;
	curFilters.flux.params.enable = false;
	setBiquadFlux(curFilters.flux);
	
	// Calibrate ADC with power stage safely enabled.
	if (!enablePowerStageForCalibration(false)) {
		curFilters.flux.params.enable = true;
		setBiquadFlux(curFilters.flux);
		fullCalibrationInProgress = false;
		allowStateChange = true;
		enterShutdown();
		return;
	}
	Delay(50);
	if (calibrateAdcOffset(500)) {
		saveAdcParams();
	} else {
		curFilters.flux.params.enable = true;
		setBiquadFlux(curFilters.flux);
		calibFailCb();
		return;
	}

	// Encoder
	calibrateEncoder();
	curFilters.flux.params.enable = true;
	setBiquadFlux(curFilters.flux);
	if (!canRunMotor()) {
		// Abort full calibration cleanly on E-stop, power loss, or HardError without marking calibrated.
		fullCalibrationInProgress = false;
		if (state != TMC_ControlState::HardError) {
			if (isEmergencyActive() || brake_failure) {
				enterShutdown();
			} else {
				enterWaitPower();
			}
		}
		return;
	}
	allowStateChange = true;
	setEncoderType(conf.motconf.enctype);
	recalibrationRequired = false;
}

// Cogging calibration pipeline moved to TMC4671_cogging.cpp

/**
 * Iterative tuning function for tuning the torque mode PI values (Asynchronous version)
 */
void TMC4671::handleStatePidAutoTune() {
	if (isEmergencyActive() || brake_failure || state == TMC_ControlState::HardError) {
		pidTuneState = PidTuneState::Init;
		if (state != TMC_ControlState::HardError) {
			enterShutdown();
		}
		return;
	}
	if (conf.motconf.motor_type == MotorType::NONE || !initialized) {
		pidTuneState = PidTuneState::Init;
		changeState(TMC_ControlState::Shutdown);
		return;
	}
	if (!hasPower() || !powerInitialized) {
		pidTuneState = PidTuneState::Init;
		this->postPowerState = TMC_ControlState::Pidautotune;
		changeState(TMC_ControlState::waitPower);
		return;
	}

	// 1. Initial Setup
	allowStateChange = false;
	if (!enablePowerStageForCalibration()) {
		pidTuneState = PidTuneState::Init;
		allowStateChange = true;
		enterShutdown();
		return;
	}
	curFilters.flux.params.enable = false;
	setBiquadFlux(curFilters.flux);
	lastPidTunePhiE = getPhiEtype();
	lastPidTuneMode = getMotionMode();
	setPhiE_ext(getPhiE());
	setPhiEtype(PhiE::ext); // Fixed phase for testing flux response
	setMotionMode(MotionMode::torque, true);
	setFluxTorque(0, 0);
	
	int32_t targetflux_i32 = std::min<int32_t>(getSafeTorqueLimit(), bangInitPower);
	if (targetflux_i32 <= 0) {
		curFilters.flux.params.enable = true;
		setBiquadFlux(curFilters.flux);
		setPhiEtype(lastPidTunePhiE);
		setMotionMode(lastPidTuneMode, true);
		pidTuneState = PidTuneState::Init;
		allowStateChange = true;
		changeState(TMC_ControlState::Shutdown);
		return;
	}
	int16_t targetflux = (int16_t)targetflux_i32;
	int16_t targetflux_p = targetflux * 0.75;
	
	tuneFluxI = 0;
	tuneFluxP = 100;
	writeReg(0x54, tuneFluxI | (tuneFluxP << 16));
	
	// 2. Atomic P-Gain Tuning (Ramp)
	while (tuneFluxP < 20000 && canRunMotor()) {
		setFluxTorque(targetflux, 0);
		Delay(50); // Stabilization delay
		int32_t flux = getActualFlux();
		if (flux > targetflux_p) break;
		
		// Adaptive steps
		tuneFluxP += (flux > targetflux * 0.5) ? 50 : 250; 
		writeReg(0x54, tuneFluxI | (tuneFluxP << 16));
	}
	
	// 3. Atomic I-Gain Tuning (Overshoot detection)
	tuneFluxI = 100;
	while (tuneFluxI < 20000 && canRunMotor()) {
		setFluxTorque(0, 0);
		Delay(100); // Settle current to zero
		if (!canRunMotor()) break;
		
		tuneMeasurePeak = 0;
		writeReg(0x54, tuneFluxI | (tuneFluxP << 16));
		setFluxTorque(targetflux, 0); // Pulse
		
		// Ultra-fast sampling loop for peak detection (Atomic)
		uint32_t start = HAL_GetTick();
		while(HAL_GetTick() - start < 50 && canRunMotor()) {
			tuneMeasurePeak = std::max<int32_t>(tuneMeasurePeak, getActualFlux());
		}
		
		// Check for overshoot target (default 4%)
		if (tuneMeasurePeak > (targetflux + (targetflux * TMC4671_ITUNE_CUTOFF))) {
			if (tuneMeasurePeak > targetflux) tuneFluxI -= (tuneFluxI > 20) ? 20 : 0; // Back off
			break;
		}
		
		tuneFluxI += (tuneMeasurePeak < targetflux * 0.95) ? 150 : 25;
	}

	// 4. Finalization & Cleanup
	setFluxTorque(0, 0);
	curFilters.flux.params.enable = true;
	setBiquadFlux(curFilters.flux);
	
	if (tuneFluxP < 20000 && tuneFluxI < 20000 && canRunMotor()) {
		pidTuneNewPids = curPids;
		pidTuneNewPids.fluxP = tuneFluxP;
		pidTuneNewPids.torqueP = tuneFluxP;
		pidTuneNewPids.fluxI = tuneFluxI;
		pidTuneNewPids.torqueI = tuneFluxI;
		setPids(pidTuneNewPids);
		CommandHandler::broadcastCommandReply(CommandReply("PID Autotune successful", 1), (uint32_t)TMC4671_commands::pidautotune, CMDtype::get);
	} else {
		// Always restore previous safe PID gains on failure or interruption to avoid leaving fluxP=20000 active.
		setPids(curPids);
		CommandHandler::broadcastCommandReply(CommandReply("PID Autotune failed or interrupted", 0), (uint32_t)TMC4671_commands::pidautotune, CMDtype::get);
	}

	setPhiEtype(lastPidTunePhiE);
	pidTuneState = PidTuneState::Init;
	if (isEmergencyActive() || brake_failure) {
		enterShutdown();
	} else if (!hasPower()) {
		enterWaitPower();
	} else {
		allowStateChange = true;
		if (motorEnabledRequested && isSetUp()) {
			setMotionMode(lastPidTuneMode, true);
			changeState(TMC_ControlState::Running);
		} else {
			stopMotor();
			changeState(TMC_ControlState::Shutdown);
		}
	}
}

#endif
