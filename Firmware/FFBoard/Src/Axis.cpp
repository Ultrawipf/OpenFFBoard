/*
 * Axis.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#include "Axis.h"
#include "voltagesense.h"
#include "TMC4671.h"
#include "MotorPWM.h"
#include "VescCAN.h"
#include "ODriveCAN.h"
#include "MotorSimplemotion.h"
#include "RmdMotorCAN.h"

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
Axis::Axis(char axis,volatile Control_t* control) :CommandHandler("axis", CLSID_AXIS), drv_chooser(MotorDriver::all_drivers),enc_chooser{Encoder::all_encoders}
{
	this->axis = axis;
	this->control = control;
	if (axis == 'X')
	{
		drv_chooser = ClassChooser<MotorDriver>(axis1_drivers);
		setInstance(0);
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS1_CONFIG, ADR_AXIS1_MAX_SPEED, ADR_AXIS1_MAX_ACCEL,
										   ADR_AXIS1_ENDSTOP, ADR_AXIS1_POWER, ADR_AXIS1_DEGREES,ADR_AXIS1_EFFECTS1,ADR_AXIS1_EFFECTS2,ADR_AXIS1_ENC_RATIO,
										   ADR_AXIS1_SPEEDACCEL_FILTER,ADR_AXIS1_POSTPROCESS1});
	}
	else if (axis == 'Y')
	{
		drv_chooser = ClassChooser<MotorDriver>(axis2_drivers);
		setInstance(1);
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS2_CONFIG, ADR_AXIS2_MAX_SPEED, ADR_AXIS2_MAX_ACCEL,
										   ADR_AXIS2_ENDSTOP, ADR_AXIS2_POWER, ADR_AXIS2_DEGREES,ADR_AXIS2_EFFECTS1,ADR_AXIS2_EFFECTS2, ADR_AXIS2_ENC_RATIO,
										   ADR_AXIS2_SPEEDACCEL_FILTER,ADR_AXIS2_POSTPROCESS1});
	}
	else if (axis == 'Z')
	{
		setInstance(2);
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS3_CONFIG, ADR_AXIS3_MAX_SPEED, ADR_AXIS3_MAX_ACCEL,
										   ADR_AXIS3_ENDSTOP, ADR_AXIS3_POWER, ADR_AXIS3_DEGREES,ADR_AXIS3_EFFECTS1,ADR_AXIS3_EFFECTS2,ADR_AXIS3_ENC_RATIO,
										   ADR_AXIS3_SPEEDACCEL_FILTER,ADR_AXIS3_POSTPROCESS1});
	}


	restoreFlash(); // Load parameters
	CommandHandler::registerCommands(); // Internal commands
	registerCommands();
	updateTorqueScaler(); // In case no flash setting has been loaded yet
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
	registerCommand("maxtorquerate", Axis_commands::maxtorquerate, "Torque rate limit in counts/ms",CMDFLAG_GET | CMDFLAG_SET);
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
	//NormalizedAxis::restoreFlash();
	// read all constants
	uint16_t value;
	if (Flash_Read(flashAddrs.config, &value)){
		this->conf = Axis::decodeConfFromInt(value);
	}else{
		pulseErrLed();
	}

	setDrvType(this->conf.drvtype);
	setEncType(this->conf.enctype);

	if (Flash_Read(flashAddrs.maxSpeed, &value)){
		this->maxSpeedDegS = value;
	}else{
		pulseErrLed();
	}
//
//	if (Flash_Read(flashAddrs.maxAccel, &value)){
//		this->maxTorqueRateMS = value;
//	}else{
//		pulseErrLed();
//	}


	uint16_t esval, power;
	if(Flash_Read(flashAddrs.endstop, &esval)) {
		fx_ratio_i = esval & 0xff;
		endstopStrength = (esval >> 8) & 0xff;
	}


	if(Flash_Read(flashAddrs.power, &power)){
		setPower(power);
	}
	uint16_t deg_t;
	if(Flash_Read(flashAddrs.degrees, &deg_t)){
		this->degreesOfRotation = deg_t & 0x7fff;
		this->invertAxis = (deg_t >> 15) & 0x1;
		setDegrees(degreesOfRotation);
	}


	uint16_t effects;
	if(Flash_Read(flashAddrs.effects1, &effects)){
		setIdleSpringStrength(effects & 0xff);
		setFxStrengthAndFilter((effects >> 8) & 0xff,damperIntensity,damperFilter);
	}else{
		setIdleSpringStrength(idlespringstrength); // Use default
	}

	if(Flash_Read(flashAddrs.effects2, &effects)){
		setFxStrengthAndFilter(effects & 0xff,frictionIntensity,frictionFilter);
		setFxStrengthAndFilter((effects >> 8) & 0xff,inertiaIntensity,inertiaFilter);
	}

	uint16_t ratio;
	if(Flash_Read(flashAddrs.encoderRatio, &ratio)){
		setGearRatio(ratio & 0xff, (ratio >> 8) & 0xff);
	}

	uint16_t filterStorage;
	if (Flash_Read(flashAddrs.speedAccelFilter, &filterStorage))
	{
		uint8_t profile = filterStorage & 0xFF;
		this->filterProfileId = profile;
		speedFilter.setFc(filterSpeedCst[this->filterProfileId].freq / filter_f);
		speedFilter.setQ(filterSpeedCst[this->filterProfileId].q / 100.0);
		accelFilter.setFc(filterAccelCst[this->filterProfileId].freq / filter_f);
		accelFilter.setQ(filterAccelCst[this->filterProfileId].q / 100.0);
	}

	uint16_t pp1;
	if(Flash_Read(flashAddrs.postprocess1, &pp1)){
		setExpo((int8_t)(pp1 & 0xff));
	}

}
// Saves parameters to flash.
void Axis::saveFlash(){
	//NormalizedAxis::saveFlash();
	Flash_Write(flashAddrs.config, Axis::encodeConfToInt(this->conf));
	Flash_Write(flashAddrs.maxSpeed, this->maxSpeedDegS);
//	Flash_Write(flashAddrs.maxAccel, (uint16_t)(this->maxTorqueRateMS));

	Flash_Write(flashAddrs.endstop, fx_ratio_i | (endstopStrength << 8));
	Flash_Write(flashAddrs.power, power);
	Flash_Write(flashAddrs.degrees, (degreesOfRotation & 0x7fff) | (invertAxis << 15));
	Flash_Write(flashAddrs.effects1, idlespringstrength | (damperIntensity << 8));
	Flash_Write(flashAddrs.effects2, frictionIntensity | (inertiaIntensity << 8));
	Flash_Write(flashAddrs.encoderRatio, gearRatio.numerator | (gearRatio.denominator << 8));

	// save CF biquad
	uint16_t filterStorage = (uint16_t)this->filterProfileId & 0xFF;
	Flash_Write(flashAddrs.speedAccelFilter, filterStorage);

	// Postprocessing
	Flash_Write(flashAddrs.postprocess1, expoValInt & 0xff);

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
		//ErrorHandler::clearError(outOfBoundsError);
	}

	// On first change to ready start a fade
	if(motorWasNotReady && drv->motorReady()){
		motorWasNotReady = false;
		startForceFadeIn(0, 1.0);
	}


	this->updateMetrics(angle);

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
	updateTorqueScaler();
#ifdef TMC4671DRIVER
	// Update hardware limits for TMC for safety
	TMC4671 *drv = dynamic_cast<TMC4671 *>(this->drv.get());
	if (drv != nullptr)
	{
		//tmclimits.pid_uq_ud = power;
		//tmclimits.pid_torque_flux = power;
		drv->setTorqueLimit(power);
	}
#endif
}


/**
 * create and setup a motor driver
 */
void Axis::setDrvType(uint8_t drvtype)
{
	if (!drv_chooser.isValidClassId(drvtype))
	{
		return;
	}
	this->drv.reset(nullptr);
	MotorDriver* drv = drv_chooser.Create((uint16_t)drvtype);
	if (drv == nullptr)
	{
		return;
	}
	this->drv = std::unique_ptr<MotorDriver>(drv);
	this->conf.drvtype = drvtype;

	// Pass encoder to driver again
	if(!this->drv->hasIntegratedEncoder()){
		this->drv->setEncoder(this->enc);
	}
#ifdef TMC4671DRIVER
	if (dynamic_cast<TMC4671 *>(drv))
	{
		setupTMC4671();
	}
#endif
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

#ifdef TMC4671DRIVER
// Special tmc setup methods
void Axis::setupTMC4671()
{
	TMC4671 *drv = static_cast<TMC4671 *>(this->drv.get());
//	drv->setAxis(axis);
	drv->setExternalEncoderAllowed(true);
	drv->restoreFlash();
	tmclimits.pid_torque_flux = getPower();
	drv->setLimits(tmclimits);
	//drv->setBiquadTorque(TMC4671Biquad(tmcbq_500hz_07q_25k));
	

	// Enable driver

	drv->setMotionMode(MotionMode::torque);
	drv->Start(); // Start thread
}
#endif


/**
 * Init the encoder and reset metrics
 */
void Axis::setEncType(uint8_t enctype)
{
	if (enc_chooser.isValidClassId(enctype) && !drv->hasIntegratedEncoder())
	{

		this->conf.enctype = (enctype);
		this->enc = std::shared_ptr<Encoder>(enc_chooser.Create(enctype)); // Make new encoder
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
 * Returns position as 16b int scaled to gamepad range
 */
int32_t Axis::getLastScaledEnc() {
	return  clip(metric.current.pos,-0x7fff,0x7fff);
}

/**
 * Changes intensity of idle spring when FFB is off
 */
int32_t Axis::updateIdleSpringForce() {
	return clip<int32_t,int32_t>((int32_t)(-metric.current.pos*idlespringscale),-idlespringclip,idlespringclip);
}

/*
 * Set the strength of the spring effect if FFB is disabled
 */
void Axis::setIdleSpringStrength(uint8_t spring){
	idlespringstrength = spring;
	idlespringclip = clip<int32_t,int32_t>((int32_t)spring*35,0,10000);
	idlespringscale = 0.5f + ((float)spring * 0.01f);
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
 * Called before HID effects are calculated
 * Should calculate always on and idle effects specific to the axis like idlespring and friction
 */
void Axis::calculateAxisEffects(bool ffb_on){
	axisEffectTorque = 0;

	if(!ffb_on){
		axisEffectTorque += updateIdleSpringForce();
	}

	// Always active damper
	if(damperIntensity != 0){
		float speedFiltered = (metric.current.speed) * (float)damperIntensity * AXIS_DAMPER_RATIO;
		axisEffectTorque -= damperFilter.process(clip<float, int32_t>(speedFiltered, -intFxClip, intFxClip));
	}

	// Always active inertia
	if(inertiaIntensity != 0){
		float accelFiltered = metric.current.accel * (float)inertiaIntensity * AXIS_INERTIA_RATIO;
		axisEffectTorque -= inertiaFilter.process(clip<float, int32_t>(accelFiltered, -intFxClip, intFxClip));
	}

	// Always active friction. Based on effectsCalculator implementation
	if(frictionIntensity != 0){
		float speed = metric.current.speed * INTERNAL_SCALER_FRICTION;
		float speedRampupCeil = 4096;
		float rampupFactor = 1.0;
		if (fabs (speed) < speedRampupCeil) {								// if speed in the range to rampup we apply a sine curve
			float phaseRad = M_PI * ((fabs (speed) / speedRampupCeil) - 0.5);// we start to compute the normalized angle (speed / normalizedSpeed@5%) and translate it of -1/2PI to translate sin on 1/2 periode
			rampupFactor = ( 1 + sin(phaseRad ) ) / 2;						// sin value is -1..1 range, we translate it to 0..2 and we scale it by 2
		}
		int8_t sign = speed >= 0 ? 1 : -1;
		float force = (float)frictionIntensity * rampupFactor * sign * INTERNAL_AXIS_FRICTION_SCALER * 32;
		axisEffectTorque -= frictionFilter.process(clip<float, int32_t>(force, -intFxClip, intFxClip));
	}

}

/**
 * Changes the ratio of effects to endstop strength. 255 = same strength, 0 = no effects
 */
void Axis::setFxRatio(uint8_t val) {
	fx_ratio_i = val;
	updateTorqueScaler();
}

/**
 * Resets the metrics and filters
 */
void Axis::resetMetrics(float new_pos= 0) { // pos is degrees
	metric.current = metric_t();
	metric.current.posDegrees = new_pos;
	std::tie(metric.current.pos,metric.current.pos_f) = scaleEncValue(new_pos, degreesOfRotation);
	metric.previous = metric_t();
	// Reset filters
	speedFilter.calcBiquad();
	accelFilter.calcBiquad();
}

/**
 * Updates metrics
 */
void Axis::updateMetrics(float new_pos) { // pos is degrees
	// store old value for next metric's computing
	metric.previous = metric.current;

	metric.current.posDegrees = new_pos;
	std::tie(metric.current.pos,metric.current.pos_f) = scaleEncValue(new_pos, degreesOfRotation);


	// compute speed and accel from raw instant speed normalized
	float currentSpeed = (new_pos - metric.previous.posDegrees) * 1000.0; // deg/s
	metric.current.speed = speedFilter.process(currentSpeed);
	metric.current.accel = accelFilter.process((currentSpeed - _lastSpeed))* 1000.0; // deg/s/s
	_lastSpeed = currentSpeed;

}



uint16_t Axis::getPower(){
	return power;
}

/**
 * Calculates an exponential torque correction curve
 */
int32_t Axis::calculateExpoTorque(int32_t torque){
	float torquef = (float)torque / (float)0x7fff; // This down and upscaling may introduce float artifacts. Do this before scaling down.
	if(torquef < 0){
		return -powf(-torquef,expo) * 0x7fff;
	}else{
		return powf(torquef,expo) * 0x7fff;
	}
}

void  Axis::updateTorqueScaler() {
	effect_margin_scaler = ((float)fx_ratio_i/255.0);
	torqueScaler = ((float)power / (float)0x7fff);
}

float Axis::getTorqueScaler(){
	return torqueScaler;
}


int32_t Axis::getTorque() { return metric.current.torque; }

bool Axis::isInverted() {
	return invertAxis;
}

/**
 * Calculate soft endstop effect
 */
int16_t Axis::updateEndstop(){
	int8_t clipdir = cliptest<int32_t,int32_t>(metric.current.pos, -0x7fff, 0x7fff);
	if(clipdir == 0){
		return 0;
	}
	float addtorque = clipdir*metric.current.posDegrees - (float)this->degreesOfRotation/2.0; // degress of rotation counts total range so multiply by 2
	addtorque *= (float)endstopStrength * endstopGain; // Apply endstop gain for stiffness.
	addtorque *= -clipdir;

	return clip<int32_t,int32_t>(addtorque,-0x7fff,0x7fff);
}

void Axis::setEffectTorque(int32_t torque) {
	effectTorque = torque;
}

/** pass in ptr to receive the sum of the effects + endstop torque
 *  return true if torque has changed
*/

bool Axis::updateTorque(int32_t* totalTorque) {

	if(abs(effectTorque) >= 0x7fff){
		pulseClipLed();
	}

	// Scale effect torque
	int32_t torque = effectTorque; // Game effects
	if(expo != 1){
		torque = calculateExpoTorque(torque);
	}
	torque *= effect_margin_scaler;
	torque += axisEffectTorque; // Independent effects
	torque += updateEndstop();
	torque *= torqueScaler; // Scale to power


	// TODO speed and accel limiters
	if(maxSpeedDegS > 0){

		float torqueSign = torque > 0 ? 1 : -1; // Used to prevent metrics against the force to go into the limiter
		// Speed. Mostly tuned...
		//spdlimiterAvg.addValue(metric.current.speed);
		float speedreducer = (float)((metric.current.speed*torqueSign) - (float)maxSpeedDegS) *  ((float)0x7FFF / maxSpeedDegS);
		spdlimitreducerI = clip<float,int32_t>( spdlimitreducerI + ((speedreducer * speedLimiterI) * torqueScaler),0,power);

		// Accel limit. Not really useful. Maybe replace with torque slew rate limit?
//		float accreducer = (float)((metric.current.accel*torqueSign) - (float)maxAccelDegSS) * getAccelScalerNormalized();
//		acclimitreducerI = clip<float,int32_t>( acclimitreducerI + ((accreducer * 0.02) * torqueScaler),0,power);


		// Only reduce torque. Don't invert it to prevent oscillation
		float torqueReduction = speedreducer * speedLimiterP + spdlimitreducerI;// accreducer * 0.025 + acclimitreducerI
		if(torque > 0){
			torqueReduction = clip<float,int32_t>(torqueReduction,0,torque);
		}else{
			torqueReduction = clip<float,int32_t>(-torqueReduction,torque,0);
		}

		torque -= torqueReduction;
	}
	// Torque slew rate limiter
	if(maxTorqueRateMS > 0){
		torque = clip<int32_t,int32_t>(torque, metric.previous.torque - maxTorqueRateMS,metric.previous.torque + maxTorqueRateMS);
	}
//	if(torque - metric.previous.torque)
	if(outOfBounds){
		torque = 0;
	}

	// Fade in
	if(forceFadeCurMult < 1){
		torque = torque * forceFadeCurMult;
		forceFadeCurMult += forceFadeTime / this->filter_f; // Fade time
	}

	// Torque calculated. Now sending to driver
	torque = (invertAxis) ? -torque : torque;
	metric.current.torque = torque;
	torque = clip<int32_t, int32_t>(torque, -power, power);

	bool torqueChanged = metric.current.torque != metric.previous.torque;

	if (abs(torque) == power){
		pulseClipLed();
	}

	*totalTorque = torque;
	return (torqueChanged);
}

/**
 * Starts fading in force from start to 1 over fadeTime
 */
void Axis::startForceFadeIn(float start,float fadeTime){
	this->forceFadeTime = fadeTime;
	this->forceFadeCurMult = clip<float>(start, 0, 1);
}


/**
 * Changes gamepad range in degrees for effect scaling
 */
void Axis::setDegrees(uint16_t degrees){

	degrees &= 0x7fff;
	if(degrees == 0){
		nextDegreesOfRotation = lastdegreesOfRotation;
	}else{
		lastdegreesOfRotation = degreesOfRotation;
		nextDegreesOfRotation = degrees;
	}
}


void Axis::setExpo(int val){
	val = clip(val, -127, 127);
	expoValInt = val;
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
//		if (cmd.type == CMDtype::get)
//		{
//			replies.emplace_back(degreesOfRotation);
//		}
//		else if (cmd.type == CMDtype::set)
//		{
//			setDegrees(cmd.val);
//		}
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
			replies.emplace_back(idlespringstrength);
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
			enc_chooser.replyAvailableClasses(replies,this->getEncType());
		}else if(cmd.type == CMDtype::get){
			replies.emplace_back(this->getEncType());
		}else if(cmd.type == CMDtype::set){
			this->setEncType(cmd.val);
		}
		break;

	case Axis_commands::drvtype:
		if(cmd.type == CMDtype::info){
			drv_chooser.replyAvailableClasses(replies,this->getDrvType());
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

	case Axis_commands::maxtorquerate:
		handleGetSet(cmd, replies, this->maxTorqueRateMS);
		break;

	case Axis_commands::fxratio:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(this->fx_ratio_i);
		}else if(cmd.type == CMDtype::set){
			setFxRatio(cmd.val);
		}
		break;

	case Axis_commands::curpos:
		replies.emplace_back(this->metric.current.pos);
		break;
	case Axis_commands::curtorque:
		replies.emplace_back(this->metric.current.torque);
		break;
	case Axis_commands::curspd:
		replies.emplace_back(this->metric.current.speed);
		break;
	case Axis_commands::curaccel:
		replies.emplace_back(this->metric.current.accel);
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
			this->filterProfileId = value;
			speedFilter.setFc(filterSpeedCst[this->filterProfileId].freq / filter_f);
			speedFilter.setQ(filterSpeedCst[this->filterProfileId].q / 100.0);
			accelFilter.setFc(filterAccelCst[this->filterProfileId].freq / filter_f);
			accelFilter.setQ(filterAccelCst[this->filterProfileId].q / 100.0);
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
		handleGetSetFunc(cmd, replies, expoValInt, &Axis::setExpo, this); // need to also provide the expoScaler constant
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
