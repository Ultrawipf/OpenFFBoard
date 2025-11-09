/*
 * Axis.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick, Vincent
 * 		
 */

#include "Axis.h"
#include "voltagesense.h"

// Load the driver is they are declared in targer_constants.h
#ifdef TMC4671DRIVER
#include "TMC4671.h"
#endif
#ifdef PWMDRIVER
#include "MotorPWM.h"
#endif
#ifdef ODRIVE
#include "ODriveCAN.h"
#endif
#ifdef VESC
#include "VescCAN.h"
#endif
#ifdef SIMPLEMOTION
#include "MotorSimplemotion.h"
#endif
#ifdef RMDCAN
#include "RmdMotorCAN.h"
#endif

//////////////////////////////////////////////
/*
 * Sources for class choosers are defined in MotorDriver and Encoder
 */


//////////////////////////////////////////////

ClassIdentifier Axis::info = {
	.name = "Axis",
	.id = CLSID_AXIS, // 1
	.visibility = ClassVisibility::visible};

/**
 * List of motor drivers available to the first axis
 */
const std::vector<class_entry<MotorDriver>> Axis::axis1_drivers =
{
	add_class<MotorDriver, MotorDriver>(0),

#ifdef TMC4671DRIVER
	add_class<TMC_1, MotorDriver>(1),
#endif
#ifdef PWMDRIVER
	add_class<MotorPWM, MotorDriver>(4),
#endif
#ifdef ODRIVE
	add_class<ODriveCAN1,MotorDriver>(5),
#endif
#ifdef VESC
	add_class<VESC_1,MotorDriver>(7),
#endif
#ifdef SIMPLEMOTION
	add_class<MotorSimplemotion1,MotorDriver>(9),
#endif
#ifdef RMDCAN
	add_class<RmdMotorCAN1,MotorDriver>(11),
#endif
};

/**
 * List of motor drivers available to the second axis
 */
const std::vector<class_entry<MotorDriver>> Axis::axis2_drivers =
{
	add_class<MotorDriver, MotorDriver>(0),

#ifdef TMC4671DRIVER
	add_class<TMC_2, MotorDriver>(2),
#endif
#ifdef PWMDRIVER
	add_class<MotorPWM, MotorDriver>(4),
#endif
#ifdef ODRIVE
	add_class<ODriveCAN2,MotorDriver>(6),
#endif
#ifdef VESC
	add_class<VESC_2,MotorDriver>(8),
#endif
#ifdef RMDCAN
	add_class<RmdMotorCAN2,MotorDriver>(12),
#endif
//#ifdef SIMPLEMOTION
//	add_class<MotorSimplemotion2,MotorDriver>(10), // TODO this likely does not work reliably with a single uart port and multiple devices
//#endif
};


/**
 * Axis class manages motor drivers and passes effect torque to the motor drivers
 */
Axis::Axis(char axis,volatile Control_t* control) :CommandHandler("axis", CLSID_AXIS), driverChooser(MotorDriver::all_drivers),encoderChooser{Encoder::all_encoders}
{

#ifdef USE_DSP_FUNCTIONS
	speedLimiterPID.Kp = speedLimiterP;
	speedLimiterPID.Ki = speedLimiterI;
	speedLimiterPID.Kd = 0.0f;
	arm_pid_init_f32(&speedLimiterPID, 1);
#endif

	this->axis = axis;
	this->control = control;
	if (axis == 'X')
	{
		driverChooser = ClassChooser<MotorDriver>(axis1_drivers);
		setInstance(0);
		this->flashAddresses = AxisFlashAddresses({ADR_AXIS1_CONFIG, ADR_AXIS1_MAX_SPEED, ADR_AXIS1_MAX_ACCEL, ADR_AXIS1_MAX_SLEWRATE_DRV,
										   ADR_AXIS1_ENDSTOP, ADR_AXIS1_POWER, ADR_AXIS1_DEGREES,ADR_AXIS1_EFFECTS1,ADR_AXIS1_EFFECTS2,ADR_AXIS1_ENC_RATIO,
										   ADR_AXIS1_SPEEDACCEL_FILTER,ADR_AXIS1_POSTPROCESS1});
	}
	else if (axis == 'Y')
	{
		driverChooser = ClassChooser<MotorDriver>(axis2_drivers);
		setInstance(1);
		this->flashAddresses = AxisFlashAddresses({ADR_AXIS2_CONFIG, ADR_AXIS2_MAX_SPEED, ADR_AXIS2_MAX_ACCEL, ADR_AXIS2_MAX_SLEWRATE_DRV,
										   ADR_AXIS2_ENDSTOP, ADR_AXIS2_POWER, ADR_AXIS2_DEGREES,ADR_AXIS2_EFFECTS1,ADR_AXIS2_EFFECTS2, ADR_AXIS2_ENC_RATIO,
										   ADR_AXIS2_SPEEDACCEL_FILTER,ADR_AXIS2_POSTPROCESS1});
	}
	else if (axis == 'Z')
	{
		setInstance(2);
	this->flashAddresses = AxisFlashAddresses({ADR_AXIS3_CONFIG, ADR_AXIS3_MAX_SPEED, ADR_AXIS3_MAX_ACCEL, ADR_AXIS3_MAX_SLEWRATE_DRV,
										   ADR_AXIS3_ENDSTOP, ADR_AXIS3_POWER, ADR_AXIS3_DEGREES,ADR_AXIS3_EFFECTS1,ADR_AXIS3_EFFECTS2,ADR_AXIS3_ENC_RATIO,
										   ADR_AXIS3_SPEEDACCEL_FILTER,ADR_AXIS3_POSTPROCESS1});
	}

	// Initialize equalizer filters
	/*for (uint8_t idx = 0; idx < num_eq_bands; idx++) {
		eqFilters[idx].setBiquad(BiquadType::peak, eq_frequencies[idx] / filter_f, 1.0, 0.0);
	}*/

	CommandHandler::registerCommands(); // Internal commands
	registerCommands();
	restoreFlash(); // Load parameters
}

Axis::~Axis()
{

}

const ClassIdentifier Axis::getInfo() {

	return info;
}

void Axis::registerCommands(){
	registerCommand("power", Axis_commands::power, "Overall force strength",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("degrees", Axis_commands::degrees, "Rotation range in deg",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("esgain", Axis_commands::esgain, "Endstop stiffness",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("zeroenc", Axis_commands::zeroenc, "Zero axis",CMDFLAG_GET);
	registerCommand("invert", Axis_commands::invert, "Invert axis",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("idlespring", Axis_commands::idlespring, "Idle spring strength",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("axisdamper", Axis_commands::axisdamper, "Independent damper effect",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("axisfriction", Axis_commands::axisfriction, "Independent friction effect",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("axisinertia", Axis_commands::axisinertia, "Independent inertia effect",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("enctype", Axis_commands::enctype, "Encoder type get/set/list",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("drvtype", Axis_commands::drvtype, "Motor driver type get/set/list",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("pos", Axis_commands::pos, "Encoder position",CMDFLAG_GET);
	registerCommand("maxspeed", Axis_commands::maxspeed, "Speed limit in deg/s",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("slewrate", Axis_commands::slewrate, "Torque rate limit in counts/ms",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("fxratio", Axis_commands::fxratio, "Effect ratio. Reduces game effects excluding endstop. 255=100%",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("curtorque", Axis_commands::curtorque, "Axis torque",CMDFLAG_GET);
	registerCommand("curpos", Axis_commands::curpos, "Axis position",CMDFLAG_GET);
	registerCommand("curspd", Axis_commands::curspd, "Axis speed",CMDFLAG_GET);
	registerCommand("curaccel", Axis_commands::curaccel, "Axis accel",CMDFLAG_GET);
	registerCommand("reduction", Axis_commands::reductionScaler, "Encoder to axis gear reduction (val / adr) 1-256",CMDFLAG_GET | CMDFLAG_SETADR);
	registerCommand("filterProfile_id", Axis_commands::filterProfileId, "Biquad filter profile for speed and accel", CMDFLAG_GET | CMDFLAG_SET);
	//Can only read exact filter settings
	registerCommand("filterSpeed", Axis_commands::filterSpeed, "Biquad filter freq and q*100 for speed", CMDFLAG_GET);
	registerCommand("filterAccel", Axis_commands::filterAccel, "Biquad filter freq and q*100 for accel", CMDFLAG_GET);

	registerCommand("cpr", Axis_commands::cpr, "Reported encoder CPR",CMDFLAG_GET);
	registerCommand("expo", Axis_commands::expo, "Exponential curve correction (x^(val/exposcale)+1)", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("exposcale", Axis_commands::exposcale, "Scaler constant for expo", CMDFLAG_GET);
}

/*
 * Read parameters from flash and restore settings
 */
void Axis::restoreFlash(){
	// TODO: This seems to be a remnant of a previous architecture (NormalizedAxis). Confirm and remove.
	// read all constants
	uint16_t value;
	if (Flash_Read(flashAddresses.config, &value)){
		this->conf = Axis::decodeConfFromInt(value);
	}else{
		pulseErrLed();
	}

	setDrvType(this->conf.drvtype);
	setEncType(this->conf.enctype);

	if (Flash_Read(flashAddresses.maxSpeed, &value)){
		this->maxSpeedDegS = value;
	}else{
		pulseErrLed();
	}

	// save the max torque for the slew rate
	if (Flash_Read(flashAddresses.maxAccel, &value)){
		this->maxTorqueRateMS = value;
	}else{
		pulseErrLed();
	}

	// save the max torque for the slew rate
	if (Flash_Read(flashAddresses.maxSlewRateDrv, &value)){
		this->maxSlewRate_Driver = value;
	}else{
		pulseErrLed();
	}


	uint16_t endstopRawValue, power;
	if(Flash_Read(flashAddresses.endstop, &endstopRawValue)) {
		setEffectRatio(endstopRawValue & 0xff);
		endstopStrength = (endstopRawValue >> 8) & 0xff;
	}


	if(Flash_Read(flashAddresses.power, &power)){
		setPower(power);
	}
	uint16_t degreesRawValue;
	if(Flash_Read(flashAddresses.degrees, &degreesRawValue)){
		this->degreesOfRotation = degreesRawValue & 0x7fff;
		this->invertAxis = (degreesRawValue >> 15) & 0x1;
		setDegrees(degreesOfRotation);
	}


	uint16_t effects;
	if(Flash_Read(flashAddresses.effects1, &effects)){
		setIdleSpringStrength(effects & 0xff);
		setFxStrengthAndFilter((effects >> 8) & 0xff,damperIntensity,damperFilter);
	}else{
		setIdleSpringStrength(idleSpringStrength); // Use default
	}

	if(Flash_Read(flashAddresses.effects2, &effects)){
		setFxStrengthAndFilter(effects & 0xff,frictionIntensity,frictionFilter);
		setFxStrengthAndFilter((effects >> 8) & 0xff,inertiaIntensity,inertiaFilter);
	}

	uint16_t ratio;
	if(Flash_Read(flashAddresses.encoderRatio, &ratio)){
		setGearRatio(ratio & 0xff, (ratio >> 8) & 0xff);
	}

	uint16_t filterStorage;
	if (Flash_Read(flashAddresses.speedAccelFilter, &filterStorage))
	{
		uint8_t profile = filterStorage & 0xFF;
		this->filterProfileId = profile;
		speedFilter.setFc(filterSpeedCst[this->filterProfileId].freq / filter_f);
		speedFilter.setQ(filterSpeedCst[this->filterProfileId].q / 100.0);
		accelFilter.setFc(filterAccelCst[this->filterProfileId].freq / filter_f);
		accelFilter.setQ(filterAccelCst[this->filterProfileId].q / 100.0);
	}

	uint16_t pp1;
	if(Flash_Read(flashAddresses.postprocess1, &pp1)){
		setExpo((int8_t)(pp1 & 0xff));
	}

}
// Saves parameters to flash.
void Axis::saveFlash(){
	// TODO: This seems to be a remnant of a previous architecture (NormalizedAxis). Confirm and remove.
	Flash_Write(flashAddresses.config, Axis::encodeConfToInt(this->conf));
	Flash_Write(flashAddresses.maxSpeed, this->maxSpeedDegS);
	Flash_Write(flashAddresses.maxAccel, (uint16_t)(this->maxTorqueRateMS));
	Flash_Write(flashAddresses.maxSlewRateDrv, (uint16_t)(this->maxSlewRate_Driver));

	Flash_Write(flashAddresses.endstop, effectRatio | (endstopStrength << 8));
	Flash_Write(flashAddresses.power, power);
	Flash_Write(flashAddresses.degrees, (degreesOfRotation & 0x7fff) | (invertAxis << 15));
	Flash_Write(flashAddresses.effects1, idleSpringStrength | (damperIntensity << 8));
	Flash_Write(flashAddresses.effects2, frictionIntensity | (inertiaIntensity << 8));
	Flash_Write(flashAddresses.encoderRatio, gearRatio.numerator | (gearRatio.denominator << 8));

	// save CF biquad
	uint16_t filterStorage = (uint16_t)this->filterProfileId & 0xFF;
	Flash_Write(flashAddresses.speedAccelFilter, filterStorage);

	// Postprocessing
	Flash_Write(flashAddresses.postprocess1, expoValue & 0xff);

}


uint8_t Axis::getDrvType(){
	return (uint8_t)this->conf.drvtype;
}

uint8_t Axis::getEncType(){
	if(drv->hasIntegratedEncoder()){
		return 255;
	}
	return (uint8_t)this->conf.enctype;
}


void Axis::setPos(uint16_t val)
{
	startForceFadeIn(0.25,0.5);
	if(this->drv != nullptr){
		drv->getEncoder()->setPos(val);
	}
}

MotorDriver* Axis::getDriver(){
	return drv.get();
}

Encoder* Axis::getEncoder(){
	return drv->getEncoder();
}

/**
 * Called from FFBWheel->Update() via AxesManager->Update()
 */
void Axis::prepareForUpdate(){
	if (drv == nullptr){
		pulseErrLed();
		return;
	}

	// TODO: The motorReady() check was commented out. Review if this is still the desired behavior or if it should be restored.
	//if (!drv->motorReady()) return;

	float angle = getEncAngle(this->drv->getEncoder());

	// Scale encoder value to set rotation range
	// Update a change of range only when new range is within valid range
	// if degree change, compute the SpeedScaler, it depends on degreesOfRotation
	if (nextDegreesOfRotation != degreesOfRotation){
		int32_t scaledEnc;
		std::tie(scaledEnc,std::ignore) = scaleEncValue(angle, nextDegreesOfRotation);
		if(abs(scaledEnc) < 0x7fff){
			degreesOfRotation = nextDegreesOfRotation;
		}

	}


	// scaledEnc now gets inverted if necessary in updateMetrics
	int32_t scaledEnc;
	std::tie(scaledEnc,std::ignore) = scaleEncValue(angle, degreesOfRotation);

	if (abs(scaledEnc) > 0xffff && drv->motorReady()){
		// We are way off. Shut down
		drv->stopMotor();
		pulseErrLed();
		if(!outOfBounds){
			outOfBounds = true;
			ErrorHandler::addError(outOfBoundsError);
		}

	}else if(abs(scaledEnc) <= 0x7fff) {
		outOfBounds = false;
		// TODO: This error clearing seems to have been moved to the errorCallback. Confirm this is correct and remove this line.
		//ErrorHandler::clearError(outOfBoundsError);
	}

	// On first change to ready start a fade
	if(motorWasNotReady && drv->motorReady()){
		motorWasNotReady = false;
		startForceFadeIn(0, 1.0);
	}


	this->updateMetrics(angle);
	//this->updateHandsOffState();

}

void Axis::errorCallback(const Error &error, bool cleared){
	if(cleared && error == this->outOfBoundsError){
		drv->startMotor();
		outOfBounds = false;
	}
}


void Axis::updateDriveTorque(){
	// totalTorque = effectTorque + endstopTorque
	int32_t totalTorque;
	bool torqueChanged = updateTorque(&totalTorque);
	if (torqueChanged && drv->motorReady()){
		// Send to motor driver
		drv->turn(totalTorque);
	}
}

void Axis::setPower(uint16_t power)
{
	this->power = power;
	torqueScaler = ((float)power / (float)0x7fff);
	if (drv != nullptr)
	{
		drv->setPowerLimit(power);
	}
}


/**
 * create and setup a motor driver
 */
void Axis::setDrvType(uint8_t drvtype)
{
	if (!driverChooser.isValidClassId(drvtype))
	{
		return;
	}
	this->drv.reset(nullptr);
	MotorDriver* drv = driverChooser.Create((uint16_t)drvtype);
	if (drv == nullptr)
	{
		return;
	}
	this->drv = std::unique_ptr<MotorDriver>(drv);
	this->conf.drvtype = drvtype;
	this->maxTorqueRateMS = drv->getDrvSlewRate();

	// Pass encoder to driver again
	if(!this->drv->hasIntegratedEncoder()){
		this->drv->setEncoder(this->enc);
	}
	if (!tud_connected())
	{
		control->usb_disabled = false;
		this->usbSuspend();
	}
	else
	{
		drv->startMotor();
	}
}



/**
 * Init the encoder and reset metrics
 */
void Axis::setEncType(uint8_t enctype)
{
	if (encoderChooser.isValidClassId(enctype) && !drv->hasIntegratedEncoder())
	{

		this->conf.enctype = (enctype);
		this->enc = std::shared_ptr<Encoder>(encoderChooser.Create(enctype)); // Make new encoder
		if(drv && !drv->hasIntegratedEncoder())
			this->drv->setEncoder(this->enc);
	}else{
		this->conf.enctype = 0; // None encoder
	}

	float angle = getEncAngle(this->drv->getEncoder());
	//int32_t scaledEnc = scaleEncValue(angle, degreesOfRotation);
	// reset metrics
	this->resetMetrics(angle);

}

/**
 * Changes the internal gearRatio scaler
 * Encoder angle is multiplied with (numerator+1)/(denominator+1)
 */
void Axis::setGearRatio(uint8_t numerator,uint8_t denominator){
	this->gearRatio.denominator = denominator;
	this->gearRatio.numerator = numerator;
	this->gearRatio.gearRatio = ((float)numerator+1.0)/((float)denominator+1.0);
}

/**
 * Returns a scaled encoder value between -0x7fff and 0x7fff with a range of degrees and a float between -1 and 1
 * Takes an encoder angle in degrees
 */

std::pair<int32_t,float> Axis::scaleEncValue(float angle, uint16_t degrees){
	if (degrees == 0){
		return std::make_pair<int32_t,float>(0x7fff,0.0);
	}

	int32_t val = (0xffff / (float)degrees) * angle;
	float val_f = (2.0 / (float)degrees) * angle;

	return std::make_pair(val,val_f);
}

/**
 * Returns the encoder position in degrees
 */
float Axis::getEncAngle(Encoder *enc){
	if(enc != nullptr){
		float pos = 360.0 * enc->getPos_f() * gearRatio.gearRatio;
		if (isInverted()){
			pos= -pos;
		}
		return pos;
	}
	else{
		return 0;
	}
}

/**
 * Stops the motor driver and sets torque to 0
 */
void Axis::emergencyStop(bool reset){
	drv->turn(0); // Send 0 torque first
	if(reset){
		startForceFadeIn();
	}
	drv->emergencyStop(reset);
	control->emergency = !reset;
}

/**
 * Disables torque and motor driver
 */
void Axis::usbSuspend(){
	if (drv != nullptr){
		drv->turn(0);
		drv->stopMotor();
	}
}

/**
 * Enables motor driver
 */
void Axis::usbResume(){
	startForceFadeIn();
	if (drv != nullptr){
		drv->startMotor();
	}
}



metric_t* Axis::getMetrics() {
	return &metric.current;
}

/**
 * Returns position as int scaled to gamepad range
 */
int32_t Axis::getLastScaledEnc() {
	return  clip(metric.current.pos_f * 0x7fffffff,-0x7fffffff,0x7fffffff); // Calc from float pos
}

/**
 * Changes intensity of idle spring when FFB is off
 */
int32_t Axis::updateIdleSpringForce() {
	return clip<int32_t,int32_t>((int32_t)(-metric.current.pos_scaled_16b*idleSpringScale),-idleSpringClip,idleSpringClip);
}

/*
 * Set the strength of the spring effect if FFB is disabled
 */
void Axis::setIdleSpringStrength(uint8_t spring){
	idleSpringStrength = spring;
	idleSpringClip = clip<int32_t,int32_t>((int32_t)spring*35,0,10000);
	idleSpringScale = 0.5f + ((float)spring * 0.01f);
}

/**
 * Sets friction, inertia or damper values and resets filter
 */
void Axis::setFxStrengthAndFilter(uint8_t val,uint8_t& valToSet, Biquad& filter){
	if(valToSet == 0 && val != 0)
		filter.calcBiquad();

	valToSet = val;
}

/**
 * Calculates the internal mechanical effects (damper, friction, inertia) that are always active.
 * Called before HID effects are calculated.
 * Also calculates idle spring when FFB is inactive.
 */
void Axis::calculateMechanicalEffects(bool ffb_on){
	mechanicalEffectTorque = 0;

	if(!ffb_on){
		mechanicalEffectTorque += updateIdleSpringForce();
	}

	// Always active damper
	if(damperIntensity != 0){
		float speedFiltered = (metric.current.speed) * (float)damperIntensity * AXIS_DAMPER_RATIO;
		mechanicalEffectTorque -= damperFilter.process(clip<float, int32_t>(speedFiltered, -internalFxClip, internalFxClip));
	}

	// Always active inertia
	if(inertiaIntensity != 0){
		float accelFiltered = metric.current.accel * (float)inertiaIntensity * AXIS_INERTIA_RATIO;
		mechanicalEffectTorque -= inertiaFilter.process(clip<float, int32_t>(accelFiltered, -internalFxClip, internalFxClip));
	}

	// Always active friction. Based on effectsCalculator implementation
	if(frictionIntensity != 0){
		float speed = metric.current.speed * INTERNAL_SCALER_FRICTION;
		float speedRampupCeil = 4096;
		float rampupFactor = 1.0;
		if (fabs (speed) < speedRampupCeil) {								// if speed in the range to rampup we apply a sine curve
#ifdef USE_DSP_FUNCTIONS
			float phaseRad = PI * ((fabsf (speed) / speedRampupCeil) - 0.5f);// we start to compute the normalized angle (speed / normalizedSpeed@5%) and translate it of -1/2PI to translate sin on 1/2 periode
			rampupFactor = ( 1.0f + arm_sin_f32(phaseRad ) ) / 2.0f;			// sin value is -1..1 range, we translate it to 0..2 and we scale it by 2
#else
			float phaseRad = M_PI * ((fabsf (speed) / speedRampupCeil) - 0.5f);// we start to compute the normalized angle (speed / normalizedSpeed@5%) and translate it of -1/2PI to translate sin on 1/2 periode
			rampupFactor = ( 1.0f + sinf(phaseRad ) ) / 2.0f;			// sin value is -1..1 range, we translate it to 0..2 and we scale it by 2
#endif
		}
		int8_t sign = speed >= 0 ? 1 : -1;
		float force = (float)frictionIntensity * rampupFactor * sign * INTERNAL_AXIS_FRICTION_SCALER * 32;
		mechanicalEffectTorque -= frictionFilter.process(clip<float, int32_t>(force, -internalFxClip, internalFxClip));
	}

}

/**
 * Changes the ratio of effects to endstop strength. 255 = same strength, 0 = no effects
 */
void Axis::setEffectRatio(uint8_t val) {
	effectRatio = val;
	effectRatioScaler = ((float)effectRatio/255.0);
}

/**
 * Resets the metrics and filters
 */
void Axis::resetMetrics(float new_pos= 0) { // pos is degrees
	metric.current = metric_t();
	metric.current.posDegrees = new_pos;
	std::tie(metric.current.pos_scaled_16b,metric.current.pos_f) = scaleEncValue(new_pos, degreesOfRotation);
	metric.previous = metric_t();
	// Reset filters
	speedFilter.calcBiquad();
	accelFilter.calcBiquad();

#ifdef USE_DSP_FUNCTIONS
	arm_pid_reset_f32(&speedLimiterPID);	// reset the PID limit
#endif
}

/**
 * Updates metrics
 */
void Axis::updateMetrics(float new_pos) { // pos is degrees
	// store old value for next metric's computing
	metric.previous = metric.current;

	metric.current.posDegrees = new_pos;
	std::tie(metric.current.pos_scaled_16b,metric.current.pos_f) = scaleEncValue(new_pos, degreesOfRotation);


	// compute speed and accel from raw instant speed normalized
	float currentSpeed = (new_pos - metric.previous.posDegrees) * this->filter_f; // deg/s
	metric.current.speed = speedFilter.process(currentSpeed);
	metric.current.accel = accelFilter.process((currentSpeed - previousFrameSpeed))* this->filter_f; // deg/s/s
	previousFrameSpeed = currentSpeed;

}



uint16_t Axis::getPower(){
	return power;
}

/**
 * Calculates an exponential torque correction curve and scale for FFBEffect
 */
int32_t Axis::calculateExpoTorque(int32_t torque){
	float torquef = (float)torque / (float)0x7fff; // This down and upscaling may introduce float artifacts. Do this before scaling down.
	if(torquef < 0){
		return -powf(-torquef,expo) * 0x7fff;
	}else{
		return powf(torquef,expo) * 0x7fff;
	}
}

int64_t Axis::calculateFFBTorque() {

	int64_t torque = this->ffbEffectTorque;

	// Compute scaler
	torque *= effectRatioScaler;

	return torque * 0x7fff;
}

int32_t Axis::getTorque() { return metric.current.torque; } // Fix: move from previous to current

bool Axis::isInverted() {
	return invertAxis;
}

/**
 * Calculate soft endstop effect
 */
int32_t Axis::calculateEndstopTorque(){
	// TODO Check the type int8_t and the range clipping is -0x7fff..0x7fff
	int8_t clipDirection = cliptest<int32_t,int32_t>(metric.current.pos_scaled_16b, -0x7fff, 0x7fff);
	if(clipDirection == 0){
		return 0;
	}
	float endstopTorque = clipDirection*metric.current.posDegrees - (float)this->degreesOfRotation/2.0; // degress of rotation counts total range so multiply by 2
	endstopTorque *= (float)endstopStrength * endstopGain; // Apply endstop gain for stiffness.
	endstopTorque *= -clipDirection;

	return clip<int32_t,int32_t>(endstopTorque,-0x7fff,0x7fff);
}

void Axis::setFfbEffectTorque(int64_t torque) {
	this->ffbEffectTorque = torque;
}

/** pass in ptr to receive the sum of the effects + endstop torque
 *  return true if torque has changed
*/

bool Axis::updateTorque(int32_t* totalTorque) {

	// Step 1: Process FFB torque from the game (via helper function)
	int64_t torque = calculateFFBTorque();

	// Step 2: Add internal torque effects
	torque += mechanicalEffectTorque;		// Add locally calculated effects (damper, friction, inertia).
	torque += calculateEndstopTorque();		// Add endstop force if the axis reaches its rotation limits.

	// Step 3: Apply safety and comfort limiters
	torque -= applySpeedLimiterTorque(torque);	// apply speedLimiter
	applyTorqueSlewRateLimiter(torque);		// Torque Slew Rate Limiter: 

	// Cut torque if the encoder is out of bounds (safety) or hands are off.
	//if(outOfBounds || handsOff){
	if(outOfBounds){
		torque = 0;
	}

	// Apply a fade-in effect for a smooth force ramp-up on startup.
	if(forceFadeMultiplier < 1){
		torque = torque * forceFadeMultiplier;
		forceFadeMultiplier += forceFadeDuration / this->filter_f; // Fade time
	}

	// Step 4: Prepare the final torque for the driver.
	torque *= torqueScaler;								// Apply the main power setting ("force volume").
	torque = (invertAxis) ? -torque : torque;			// Invert axis if required.

	// clip and alert clipping
	int32_t torqueAfterClipping = clip<int32_t, int32_t>(torque, -power, power);
	if (torqueAfterClipping != torque){
		pulseClipLed();
	}

	// Store the actually applied torque for the next iteration (used by the slew rate limiter).
	metric.current.torque = torqueAfterClipping; 

	// return result
	*totalTorque = torqueAfterClipping;
	return (metric.current.torque != metric.previous.torque);
}


void Axis::applyTorqueSlewRateLimiter(int64_t& torque)
{
	// Limits the rate of change of the torque (slew rate), to smooths out sudden changes in torque.
	// Essential for a natural feel and to prevent "clanking" noises.
	if(maxTorqueRateMS == 0) {
		return; // Limiter is disabled
	}

	// This prevents sudden torque jumps, resulting in a smoother feel.
	const int64_t previousTorque = metric.previous.torque;
	const int64_t maxTorqueChange = maxTorqueRateMS;

	// The torque is clipped to be within the range of [previous torque - limit, previous torque + limit].
	torque = clip<int64_t>(torque, previousTorque - maxTorqueChange, previousTorque + maxTorqueChange);
		}

int64_t Axis::applySpeedLimiterTorque(int64_t& torque){
	// Speed Limiter: A PI controller to reduce torque when speed exceeds maxSpeedDegS.
	// The limiter only acts when torque is applied in the direction of movement.

	// if limiter is disabled, return
	if(maxSpeedDegS <= 0) {
		return 0;
	}

	int64_t resultTorque = 0;

#ifdef USE_DSP_FUNCTIONS
	float effectiveSpeed = metric.current.speed * (torque > 0 ? 1.0f : -1.0f);
	if (effectiveSpeed > maxSpeedDegS)
	{
		// --- PI Controller Logic ---
		// 1. Calculate the error term (how much we are over the speed limit).
		float speedError = effectiveSpeed - maxSpeedDegS;

		// 2. Calculate the total reduction amount using the PID controller.
		float reductionAmount = arm_pid_f32(&speedLimiterPID, speedError);

		// 3. Apply the reduction to the main torque.
		// We must only reduce the magnitude of the torque, not invert it.
		reductionAmount = clip(reductionAmount, 0.0f, fabsf((float)torque));
		if(torque > 0) {
			resultTorque = reductionAmount;
		} else {
			resultTorque = -reductionAmount;
		}
	} else {
		arm_pid_reset_f32(&speedLimiterPID); // Reset PID if not active
	}
#else
	float torqueSign = torque > 0 ? 1 : -1; // Used to prevent metrics against the force to go into the limiter
	// Speed. Mostly tuned...
	//spdlimiterAvg.addValue(metric.current.speed);
	float speedreducer = (float)((metric.current.speed*torqueSign) - (float)maxSpeedDegS) *  ((float)0x7FFF / maxSpeedDegS);
	speedLimitReducerI = clip<float,int32_t>( speedLimitReducerI + ((speedreducer * speedLimiterI) * torqueScaler),0,power);

	// Only reduce torque. Don't invert it to prevent oscillation
	float torqueReduction = speedreducer * speedLimiterP + speedLimitReducerI;// accreducer * 0.025 + acclimitreducerI
	if(torque > 0){
		resultTorque = clip<float,int64_t>(torqueReduction,0,torque);
	}else{
		resultTorque = clip<float,int64_t>(-torqueReduction,torque,0);
	}
#endif

	return resultTorque;
}

void Axis::updateSamplerate(float newSamplerate){
	this->filter_f = newSamplerate;
	this->updateFilters(this->filterProfileId); // Recalculate filters
}

void Axis::updateFilters(uint8_t profileId){
	this->filterProfileId = profileId;
	speedFilter.setFc(filterSpeedCst[this->filterProfileId].freq / filter_f);
	speedFilter.setQ(filterSpeedCst[this->filterProfileId].q / 100.0);
	accelFilter.setFc(filterAccelCst[this->filterProfileId].freq / filter_f);
	accelFilter.setQ(filterAccelCst[this->filterProfileId].q / 100.0);
	damperFilter.setFc(filterDamperCst.freq/filter_f);
	inertiaFilter.setFc(filterInertiaCst.freq/filter_f);
	frictionFilter.setFc(filterFrictionCst.freq/filter_f);
}

/**
 * Starts fading in force from start to 1 over fadeTime
 */
void Axis::startForceFadeIn(float start,float fadeTime){
	this->forceFadeDuration = fadeTime;
	this->forceFadeMultiplier = clip<float>(start, 0, 1);
}


/**
 * Changes gamepad range in degrees for effect scaling
 */
void Axis::setDegrees(uint16_t degrees){

	degrees &= 0x7fff;
	if(degrees == 0){
		nextDegreesOfRotation = previousDegreesOfRotation;
	}else{
		previousDegreesOfRotation = degreesOfRotation;
		nextDegreesOfRotation = degrees;
	}
}


void Axis::setExpo(int val){
	val = clip(val, -127, 127);
	expoValue = val;
	if(val == 0){
		expo = 1; // Explicitly force expo off
		return;
	}
	float valF = abs((float)val / expoScaler);
	if(val < 0){
		expo = 1.0f/(1.0f+valF);
	}else{
		expo = 1+valF;
	}
}

CommandStatus Axis::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<Axis_commands>(cmd.cmdId)){

	case Axis_commands::power:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(this->power);
		}
		else if (cmd.type == CMDtype::set)
		{
			setPower(cmd.val);
		}
		break;

	case Axis_commands::degrees:
		handleGetSetFunc(cmd, replies, degreesOfRotation, &Axis::setDegrees,this);
		break;

	case Axis_commands::esgain:
		handleGetSet(cmd, replies, this->endstopStrength);
		break;

	case Axis_commands::zeroenc:
		this->setPos(0);
		break;

	case Axis_commands::invert:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(invertAxis ? 1 : 0);
		}
		else if (cmd.type == CMDtype::set)
		{
			invertAxis = cmd.val >= 1 ? true : false;
			resetMetrics(-metric.current.posDegrees);
		}
		break;

	case Axis_commands::idlespring:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(idleSpringStrength);
		}
		else if (cmd.type == CMDtype::set)
		{
			setIdleSpringStrength(cmd.val);
		}
		break;

	case Axis_commands::axisdamper:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(damperIntensity);
		}
		else if (cmd.type == CMDtype::set)
		{
			setFxStrengthAndFilter(cmd.val,this->damperIntensity,this->damperFilter);
		}
		break;

	case Axis_commands::axisfriction:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(frictionIntensity);
		}
		else if (cmd.type == CMDtype::set)
		{
			setFxStrengthAndFilter(cmd.val,this->frictionIntensity,this->frictionFilter);
		}
		break;

	case Axis_commands::axisinertia:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(inertiaIntensity);
		}
		else if (cmd.type == CMDtype::set)
		{
			setFxStrengthAndFilter(cmd.val,this->inertiaIntensity,this->inertiaFilter);
		}
		break;

	case Axis_commands::enctype:
		if(cmd.type == CMDtype::info){
			encoderChooser.replyAvailableClasses(replies,this->getEncType());
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(this->getEncType());
		}else if(cmd.type == CMDtype::set){
			this->setEncType(cmd.val);
		}
		break;

	case Axis_commands::drvtype:
		if(cmd.type == CMDtype::info){
			driverChooser.replyAvailableClasses(replies,this->getDrvType());
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(this->getDrvType());
		}else if(cmd.type == CMDtype::set){
			this->setDrvType(cmd.val);
		}
		break;

	case Axis_commands::pos:
		if (cmd.type == CMDtype::get && this->drv->getEncoder() != nullptr)
		{
			int32_t pos = this->drv->getEncoder()->getPos();
			replies.emplace_back(isInverted() ? -pos : pos);
		}
		else if (cmd.type == CMDtype::set && this->drv->getEncoder() != nullptr)
		{
			this->drv->getEncoder()->setPos(isInverted() ? -cmd.val : cmd.val);
		}
		else
		{
			return CommandStatus::ERR;
		}
		break;

	case Axis_commands::maxspeed:
		handleGetSet(cmd, replies, this->maxSpeedDegS);
		break;

	case Axis_commands::slewrate:
		{
			if(cmd.type == CMDtype::get){
				// If driver has a more restrictive calibrated value, update the axis limit
				if(maxSlewRate_Driver < this->maxTorqueRateMS) {
					this->maxTorqueRateMS = maxSlewRate_Driver;
				}
				replies.emplace_back(this->maxTorqueRateMS);
			}else if(cmd.type == CMDtype::set){
				this->maxTorqueRateMS = clip<uint32_t,uint32_t>(cmd.val, 0, maxSlewRate_Driver);
			}
		}
		break;

	case Axis_commands::calibrate_maxSlewRateDrv:
		{
			if(cmd.type == CMDtype::get){
				// Start calibration on driver and set awaiting flag if start is OK
				if (drv->startSlewRateCalibration()) {
					this->awaitingSlewCalibration = true;
				} else {
					// Inform user that calibration can't started
					CommandHandler::broadcastCommandReply(CommandReply("Slew rate calibration unsupported",1), (uint32_t)Axis_commands::calibrate_maxSlewRateDrv, CMDtype::get);
				}
				replies.emplace_back(1); // ack
			}
			break;
		}

	case Axis_commands::maxSlewRateDrv:
		if (cmd.type == CMDtype::get) {
			replies.emplace_back(maxSlewRate_Driver);
		}
		break;

	case Axis_commands::fxratio:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->effectRatio);
		}else if(cmd.type == CMDtype::set){
			setEffectRatio(cmd.val);
		}
		break;

	case Axis_commands::curpos:
		replies.emplace_back(this->metric.previous.pos_scaled_16b);
		break;
	case Axis_commands::curtorque:
		replies.emplace_back(getTorque());
		break;
	case Axis_commands::curspd:
		replies.emplace_back(this->metric.previous.speed);
		break;
	case Axis_commands::curaccel:
		replies.emplace_back(this->metric.previous.accel);
		break;

	case Axis_commands::reductionScaler:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(gearRatio.numerator+1,gearRatio.denominator+1);
		}else if(cmd.type == CMDtype::setat){
			setGearRatio(cmd.val-1,cmd.adr-1);
		}
		break;

	case Axis_commands::filterProfileId:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(this->filterProfileId);
		}
		else if (cmd.type == CMDtype::set)
		{
			uint32_t value = clip<uint32_t, uint32_t>(cmd.val, 0, filterSpeedCst.size()-1);
			this->updateFilters(value);
		}
		break;
	case Axis_commands::filterSpeed:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(this->filterSpeedCst[this->filterProfileId].freq,this->filterSpeedCst[this->filterProfileId].q);
		}
		break;

	case Axis_commands::filterAccel:
		if (cmd.type == CMDtype::get)
		{
			replies.emplace_back(this->filterAccelCst[this->filterProfileId].freq,this->filterAccelCst[this->filterProfileId].q);
		}
		break;

	case Axis_commands::cpr:
		if (cmd.type == CMDtype::get)
		{
			uint32_t cpr = 0;
			if(this->drv->getEncoder() != nullptr){
				cpr = this->drv->getEncoder()->getCpr();
			}
			// TODO: For TMC4671 drivers, CPR reporting might be inconsistent. Investigate if a prescale is needed or if the UI should handle the readout correction.
//#ifdef TMC4671DRIVER // CPR should be consistent with position. Maybe change TMC to prescale to encoder count or correct readout in UI
//			TMC4671 *tmcdrv = dynamic_cast<TMC4671 *>(this->drv.get()); // Special case for TMC. Get the actual encoder resolution
//			if (tmcdrv && tmcdrv->hasIntegratedEncoder())
//			{
//				cpr = tmcdrv->getEncCpr();
//			}
//#endif
			replies.emplace_back(cpr);
		}else{
			return CommandStatus::ERR;
		}
		break;

	case Axis_commands::expo:
		handleGetSetFunc(cmd, replies, expoValue, &Axis::setExpo, this); // need to also provide the expoScaler constant
		break;

	case Axis_commands::exposcale:
		handleGetSet(cmd, replies, expoScaler); // need to also provide the expoScaler constant
		break;

	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}



/*
 * Helper functions for encoding and decoding flash variables
 */
AxisConfig Axis::decodeConfFromInt(uint16_t val)
{
	// 0-6 enc, 7-12 Mot
	AxisConfig conf;
	conf.enctype = ((val)&0x3f);
	conf.drvtype = ((val >> 6) & 0x3f);
	return conf;
}

uint16_t Axis::encodeConfToInt(AxisConfig conf)
{
	uint16_t val = (uint8_t)conf.enctype & 0x3f;
	val |= ((uint8_t)conf.drvtype & 0x3f) << 6;
	return val;
}
