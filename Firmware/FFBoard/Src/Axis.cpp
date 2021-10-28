/*
 * Axis.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#include "Axis.h"
#include "voltagesense.h"


//////////////////////////////////////////////
/*
 * Sources for class choosers are defined in MotorDriver and Encoder
 */


// TODO class type for parser? (Simhub for example)
//////////////////////////////////////////////

ClassIdentifier Axis::info = {
	.name = "Axis",
	.id = 1,
	.unique = 'X',
	.hidden = false};

Axis::Axis(char axis,volatile Control_t* control) : drv_chooser(MotorDriver::all_drivers),enc_chooser{Encoder::all_encoders}
{
	// Create HID FFB handler. Will receive all usb messages directly
	this->axis = axis;
	this->control = control;
	if (axis == 'X')
	{
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS1_CONFIG, ADR_AXIS1_MAX_SPEED, ADR_AXIS1_MAX_ACCEL,ADR_AXIS1_ENDSTOP, ADR_AXIS1_POWER, ADR_AXIS1_DEGREES,ADR_AXIS1_EFFECTS1});
	}
	else if (axis == 'Y')
	{
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS2_CONFIG, ADR_AXIS2_MAX_SPEED, ADR_AXIS2_MAX_ACCEL,ADR_AXIS2_ENDSTOP, ADR_AXIS2_POWER, ADR_AXIS2_DEGREES,ADR_AXIS2_EFFECTS1});
	}
	else if (axis == 'Z')
	{
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS3_CONFIG, ADR_AXIS3_MAX_SPEED, ADR_AXIS3_MAX_ACCEL,ADR_AXIS3_ENDSTOP, ADR_AXIS3_POWER, ADR_AXIS3_DEGREES,ADR_AXIS3_EFFECTS1});
	}


	restoreFlash(); // Load parameters

}

Axis::~Axis()
{

}

const ClassIdentifier Axis::getInfo() {
	return ClassIdentifier {Axis::info.name, Axis::info.id, axis, Axis::info.hidden};
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

//	if (Flash_Read(flashAddrs.maxSpeed, &value)){
//		this->maxSpeedDegS = value;
//	}else{
//		pulseErrLed();
//	}
//
//	if (Flash_Read(flashAddrs.maxAccel, &value)){
//		this->maxTorqueRateMS = value;
//	}else{
//		pulseErrLed();
//	}


	uint16_t esval, power;
	if(Flash_Read(flashAddrs.endstop, &esval)) {
		fx_ratio_i = esval & 0xff;
		endstop_gain = (esval >> 8) & 0xff;
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
		setDamperStrength((effects >> 8) & 0xff);
	}

}
// Saves parameters to flash.
void Axis::saveFlash(){
	//NormalizedAxis::saveFlash();
	Flash_Write(flashAddrs.config, Axis::encodeConfToInt(this->conf));
//	Flash_Write(flashAddrs.maxSpeed, this->maxSpeedDegS);
//	Flash_Write(flashAddrs.maxAccel, (uint16_t)(this->maxTorqueRateMS));

	Flash_Write(flashAddrs.endstop, fx_ratio_i | (endstop_gain << 8));
	Flash_Write(flashAddrs.power, power);
	Flash_Write(flashAddrs.degrees, (degreesOfRotation & 0x7fff) | (invertAxis << 15));
	Flash_Write(flashAddrs.effects1, idlespringstrength | (damperIntensity << 8));
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
	if(this->drv != nullptr){
		drv->getEncoder()->setPos(val);
	}
}

/*
 * Called from FFBWheel->Update() via AxesManager->Update()
 */
void Axis::prepareForUpdate(){
	if (drv == nullptr){
		pulseErrLed();
		return;
	}

	if (!drv->motorReady()) return;

	float angle = getEncAngle(this->drv->getEncoder());

	// Scale encoder value to set rotation range
	// Update a change of range only when new range is within valid range
	// if degree change, compute the SpeedScaler, it depends on degreesOfRotation
	if (nextDegreesOfRotation != degreesOfRotation && abs(scaleEncValue(angle, nextDegreesOfRotation)) < 0x7fff){
		degreesOfRotation = nextDegreesOfRotation;

//		speedScalerNormalized = getNormalizedSpeedScaler(maxSpeedDegS, degreesOfRotation);
//		accelScalerNormalized = getNormalizedAccelScaler(maxAccelDegSS, degreesOfRotation);
	}


	// scaledEnc now gets inverted if necessary in updateMetrics
	int32_t scaledEnc = scaleEncValue(angle, degreesOfRotation);

	if (abs(scaledEnc) > 0xffff){
		// We are way off. Shut down
		drv->stopMotor();
		pulseErrLed();
	}

	this->updateMetrics(angle);

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
	// Update hardware limits for TMC for safety
	TMC4671 *drv = dynamic_cast<TMC4671 *>(this->drv.get());
	if (drv != nullptr)
	{
		//tmclimits.pid_uq_ud = power;
		//tmclimits.pid_torque_flux = power;
		drv->setTorqueLimit(power);
	}
}


// create and setup a motor driver
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

	if (dynamic_cast<TMC4671 *>(drv))
	{
		setupTMC4671();
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

// Special tmc setup methods
void Axis::setupTMC4671()
{
	TMC4671 *drv = static_cast<TMC4671 *>(this->drv.get());
	drv->setAxis(axis);
	drv->restoreFlash();
	tmclimits.pid_torque_flux = getPower();
	drv->setLimits(tmclimits);
	//drv->setBiquadFlux(fluxbq);
	
	if (tmcFeedForward){
		drv->setupFeedForwardTorque(torqueFFgain, torqueFFconst);
		drv->setupFeedForwardVelocity(velocityFFgain, velocityFFconst);
		drv->setFFMode(FFMode::torque);
	}
	// Enable driver

	drv->setMotionMode(MotionMode::torque);
	drv->Start(); // Start thread
}



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
	}

	float angle = getEncAngle(this->drv->getEncoder());
	//int32_t scaledEnc = scaleEncValue(angle, degreesOfRotation);
	// reset metrics
	this->resetMetrics(angle);

//	// init the speed/accel factor from default value
//	speedScalerNormalized = getNormalizedSpeedScaler(maxSpeedDegS, degreesOfRotation);
//	accelScalerNormalized = getNormalizedAccelScaler(maxAccelDegSS, degreesOfRotation);
}

/**
 * Returns a scaled encoder value between -0x7fff and 0x7fff with a range of degrees
 * Takes an encoder angle in degrees
 */

int32_t Axis::scaleEncValue(float angle, uint16_t degrees){
	if (degrees == 0){
		return 0x7fff;
	}

	int32_t val = (0xffff / (float)degrees) * angle;
	if (isInverted()){
		val= -val;
	}
	return val;
}

/**
 * Returns the encoder position in degrees
 */
float Axis::getEncAngle(Encoder *enc){
	if(enc != nullptr){
		float pos = 360.0 * enc->getPos_f();
		if (isInverted()){
			pos= -pos;
		}
		return pos;
	}
	else{
		return 0;
	}
}


void Axis::emergencyStop(){
	drv->stopMotor();
	control->emergency = true;
}

void Axis::usbSuspend(){
	if (drv != nullptr){
		drv->turn(0);
		drv->stopMotor();
	}
}

void Axis::usbResume(){
	if (drv != nullptr){
		drv->startMotor();
	}
}



metric_t* Axis::getMetrics() {
	return &metric.current;
}

float Axis::getSpeedScalerNormalized() {
	//return speedScalerNormalized;
	return (float)0x7FFF / maxSpeedDegS;
}

//float	 Axis::getAccelScalerNormalized() {
//	//return accelScalerNormalized;
//	return (float)0x7FFF / maxAccelDegSS;
//}


int32_t Axis::getLastScaledEnc() {
	return  clip(metric.current.pos,-0x7fff,0x7fff);
}


int32_t Axis::updateIdleSpringForce() {
	return clip<int32_t,int32_t>((int32_t)(-metric.current.pos*idlespringscale),-idlespringclip,idlespringclip);
}

/*
 * Set the strength of the spring effect if FFB is disabled
 */
void Axis::setIdleSpringStrength(uint8_t spring){
	idlespringstrength = spring;
	if(spring == 0){
		idle_center = false;
	}else{
		idle_center = true;
	}
	idlespringclip = clip<int32_t,int32_t>((int32_t)spring*50,0,10000);
	idlespringscale = 0.5f + ((float)spring * 0.01f);
}

void Axis::setDamperStrength(uint8_t damper){
	this->damperIntensity = damper;
}

/*
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
		float speedFiltered = (metric.current.speed) * (float)damperIntensity * 0.15 ; // 1.5
		axisEffectTorque -= clip<float, int32_t>(speedFiltered, -damperClip, damperClip);
	}
}

void Axis::setFxRatio(uint8_t val) {
	fx_ratio_i = val;
	updateTorqueScaler();
}


void Axis::resetMetrics(float new_pos= 0) { // pos is degrees
	metric.current = metric_t();
	metric.current.posDegrees = new_pos;
	metric.current.pos = scaleEncValue(new_pos, degreesOfRotation);
	metric.previous = metric_t();

}


void Axis::updateMetrics(float new_pos) { // pos is degrees
	// store old value for next metric's computing
	metric.previous = metric.current;

	metric.current.posDegrees = new_pos;
	int32_t scaled_pos = scaleEncValue(new_pos, degreesOfRotation);
	metric.current.pos = scaled_pos;

	metric.current.speedInstant = (new_pos - metric.previous.posDegrees) * 1000.0; // deg/s

	metric.current.speed = speedFilter.process(metric.current.speedInstant);

	metric.current.accelInstant = metric.current.speedInstant - metric.previous.speedInstant;
	metric.current.accel = accelFilter.process(metric.current.accelInstant); //accel_avg.getAverage(); //accel_avg.getAverage();

	metric.current.torque = 0;

//	if (calibrationInProgress) {
//		calibMaxSpeedNormalized = abs(metric.current.speed) > calibMaxSpeedNormalized ? abs(metric.current.speed) : calibMaxSpeedNormalized;
//		calibMaxAccelNormalized = abs(metric.current.accel) > calibMaxAccelNormalized ? abs(metric.current.accel) : calibMaxAccelNormalized;
//	}
}



uint16_t Axis::getPower(){
	return power;
}

void  Axis::updateTorqueScaler() {
	float effect_margin_scaler = ((float)fx_ratio_i/255.0);
	torqueScaler = ((float)power / (float)0x7fff) * effect_margin_scaler;
}

float Axis::getTorqueScaler(){
	return torqueScaler;
}


int32_t Axis::getTorque() { return metric.current.torque; }

bool Axis::isInverted() {
	return invertAxis; // TODO store in flash
}

/**
 * Calculate soft endstop effect
 */
int16_t Axis::updateEndstop(){
	int8_t clipdir = cliptest<int32_t,int32_t>(metric.current.pos, -0x7fff, 0x7fff);
	if(clipdir == 0){
		return 0;
	}
	int32_t addtorque = clip<int32_t,int32_t>(abs(metric.current.pos)-0x7fff,-0x7fff,0x7fff);
	addtorque *= (float)endstop_gain * 1.25 * torqueScaler; // Apply endstop gain for stiffness. 0.15f
	addtorque *= -clipdir;

	return clip<int32_t,int32_t>(addtorque,-0x7fff,0x7fff);
}

void Axis::setEffectTorque(int32_t torque) {
	effectTorque = torque;
}

// pass in ptr to receive the sum of the effects + endstop torque
// return true if torque is clipping
bool Axis::updateTorque(int32_t* totalTorque) {

	if(abs(effectTorque) >= 0x7fff){
		pulseClipLed();
	}

	// Scale effect torque
	effectTorque  *= torqueScaler;

	int32_t torque = effectTorque + updateEndstop();
	torque += axisEffectTorque * torqueScaler; // Updated from effect calculator

	// TODO speed and accel limiters
	if(maxSpeedDegS > 0){

		float torqueSign = torque > 0 ? 1 : -1; // Used to prevent metrics against the force to go into the limiter
		// Speed. Mostly tuned...
		spdlimiterAvg.addValue(metric.current.speedInstant);
		float speedreducer = (float)((spdlimiterAvg.getAverage()*torqueSign) - (float)maxSpeedDegS) * getSpeedScalerNormalized();
		spdlimitreducerI = clip<float,int32_t>( spdlimitreducerI + ((speedreducer * 0.015) * torqueScaler),0,power);

		// Accel limit. Not really useful. Maybe replace with torque slew rate limit?
//		float accreducer = (float)((metric.current.accel*torqueSign) - (float)maxAccelDegSS) * getAccelScalerNormalized();
//		acclimitreducerI = clip<float,int32_t>( acclimitreducerI + ((accreducer * 0.02) * torqueScaler),0,power);


		// Only reduce torque. Don't invert it to prevent oscillation
		float torqueReduction = spdlimitreducerI + speedreducer * 0.025;// accreducer * 0.025 + acclimitreducerI
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
	if(torque - metric.previous.torque)

	// Torque calculated. Now sending to driver
	torque = (invertAxis) ? -torque : torque;
	metric.current.torque = torque;
	torque = clip<int32_t, int32_t>(torque, -power, power);

	bool torqueChanged = torque != metric.previous.torque;

	if (abs(torque) == power){
		pulseClipLed();
	}

	*totalTorque = torque;
	return (torqueChanged);
}

void Axis::setDegrees(uint16_t degrees){

	degrees &= 0x7fff;
	if(degrees == 0){
		nextDegreesOfRotation = lastdegreesOfRotation;
	}else{
		lastdegreesOfRotation = degreesOfRotation;
		nextDegreesOfRotation = degrees;
	}
}


void Axis::processHidCommand(HID_Custom_Data_t *data){

	uint8_t axis = (data->addr);
	if(axis != this->axis && axis != this->axis - 'X'){
		return;
	}

	switch (data->cmd){
		case HID_CMD_FFB_STRENGTH:
			if (data->type == HidCmdType::write){
				setPower(data->data);
			}
			else if (data->type == HidCmdType::request){
				data->data = getPower();
			}
		break;

		case HID_CMD_FFB_ZERO:
			if (data->type == HidCmdType::write){
				this->setPos(0);
			}
		break;

		case HID_CMD_FFB_ESGAIN:
			if(data->type == HidCmdType::write) {
				endstop_gain = data->data;
			}
			if(data->type == HidCmdType::request){
				data->data = endstop_gain;
			}
		break;

		case HID_CMD_FFB_FXRATIO:
			if(data->type == HidCmdType::write) {
				setFxRatio(data->data);
			}
			else if(data->type == HidCmdType::request){
				data->data = fx_ratio_i;
			}
		break;

		case HID_CMD_FFB_DEGREES:
			if (data->type == HidCmdType::write){
				setDegrees(data->data);
			}
			else if (data->type == HidCmdType::request){
				data->data = degreesOfRotation;
			}
		break;
		case HID_CMD_FFB_IDLESPRING:
			if(data->type == HidCmdType::write) {
				setIdleSpringStrength(data->data);
			}
			else if(data->type == HidCmdType::request){
				data->data = idlespringstrength;
			}
		break;

		default:

		break;
	}
	return;
}


ParseStatus Axis::command(ParsedCommand *cmd, std::string *reply)
{
	if ((cmd->prefix & 0xDF) != axis){
		return ParseStatus::NOT_FOUND;
	}

	ParseStatus flag = ParseStatus::OK;

	if (cmd->cmd == "drvtype")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(this->getDrvType());
		}
		else if (cmd->type == CMDtype::set && this->drv_chooser.isValidClassId(cmd->val))
		{
			setDrvType((cmd->val));
		}
		else
		{
			*reply += drv_chooser.printAvailableClasses(this->conf.drvtype);
		}
	}
	else if (cmd->cmd == "zeroenc")
	{
		if (cmd->type == CMDtype::get)
		{
			this->setPos(0);
			*reply += "Zeroed";
		}
	}
	else if (cmd->cmd == "enctype")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(this->getEncType());
		}
		else if (cmd->type == CMDtype::set)
		{
			setEncType(cmd->val);
		}
		else
		{
			if(this->drv->hasIntegratedEncoder()){
				*reply += "255:0:"+std::string(this->drv->getInfo().name); // TODO dynamic?
			}else{
				*reply += enc_chooser.printAvailableClasses(this->conf.enctype);
			}
		}
	}
	else if (cmd->cmd == "pos")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(this->drv->getEncoder()->getPos());
		}
		else if (cmd->type == CMDtype::set && this->drv->getEncoder() != nullptr)
		{
			this->drv->getEncoder()->setPos(cmd->val);
		}
		else
		{
			flag = ParseStatus::ERR;
			*reply += "Err. Setup encoder first";
		}
	}
	else if (cmd->cmd == "maxspeed")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(maxSpeedDegS);
		}
		else if (cmd->type == CMDtype::set)
		{
			maxSpeedDegS = cmd->val;
			//speedScalerNormalized = getNormalizedSpeedScaler(maxSpeedDegS, degreesOfRotation);

		}
	}else if (cmd->cmd == "maxtorquerate")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(maxTorqueRateMS);
		}
		else if (cmd->type == CMDtype::set)
		{
			maxTorqueRateMS = cmd->val;
			//speedScalerNormalized = getNormalizedSpeedScaler(maxSpeedDegS, degreesOfRotation);

		}
	}
//	else if (cmd->cmd == "maxaccel")
//	{
//		if (cmd->type == CMDtype::get)
//		{
//			*reply += std::to_string((uint16_t)(maxAccelDegSS*accelFactor));
//		}
//		else if (cmd->type == CMDtype::set)
//		{
//			maxAccelDegSS = cmd->val / accelFactor;
//			//accelScalerNormalized = getNormalizedAccelScaler(maxAccelDegSS, degreesOfRotation);
//		}
//	}
	else if (cmd->cmd == "fxratio")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(fx_ratio_i);
		}
		else if (cmd->type == CMDtype::set)
		{
			setFxRatio(cmd->val);
		}
	}
	else if (cmd->cmd == "power")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(power);
		}
		else if (cmd->type == CMDtype::set)
		{
			setPower(cmd->val);
		}
	}
	else if (cmd->cmd == "degrees")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(degreesOfRotation);
		}
		else if (cmd->type == CMDtype::set)
		{
			setDegrees(cmd->val);
		}
	}
	else if (cmd->cmd == "esgain")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(endstop_gain);
		}
		else if (cmd->type == CMDtype::set)
		{
			endstop_gain = cmd->val;
		}
	}
	else if (cmd->cmd == "invert")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += invertAxis ? "1" : "0";
		}
		else if (cmd->type == CMDtype::set)
		{
			invertAxis = cmd->val >= 1 ? true : false;
		}
	}
	else if (cmd->cmd == "idlespring")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(idlespringstrength);
		}
		else if (cmd->type == CMDtype::set)
		{
			setIdleSpringStrength(cmd->val);
		}
	}
	else if (cmd->cmd == "axisdamper")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(damperIntensity);
		}
		else if (cmd->type == CMDtype::set)
		{
			setDamperStrength(cmd->val);
		}
	}
//	else if (cmd->cmd == "calibmetrics")
//	{
//		if (cmd->type == CMDtype::get)
//		{
//			*reply += std::to_string(getSpeedRPMFromEncValue(calibMaxSpeedNormalized, degreesOfRotation)) + ':'
//					+ std::to_string(getAccelFromEncValue(calibMaxAccelNormalized, degreesOfRotation));
//		}
//		else if (cmd->type == CMDtype::set)
//		{
//			if (cmd->val == 1) {
//				calibrationInProgress = true;
//				calibMaxAccelNormalized = 0.0;
//				calibMaxSpeedNormalized = 0;
//			} else {
//				calibrationInProgress = false;
//			}
//		}
//	}
	else{
		flag = ParseStatus::NOT_FOUND;
	}

	return flag;
};


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
