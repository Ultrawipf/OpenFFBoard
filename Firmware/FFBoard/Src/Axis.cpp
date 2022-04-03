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


//////////////////////////////////////////////

ClassIdentifier Axis::info = {
	.name = "Axis",
	.id = CLSID_AXIS, // 1
	.visibility = ClassVisibility::visible};

Axis::Axis(char axis,volatile Control_t* control) :CommandHandler("axis", CLSID_AXIS), drv_chooser(MotorDriver::all_drivers),enc_chooser{Encoder::all_encoders}
{
	// Create HID FFB handler. Will receive all usb messages directly
	this->axis = axis;
	this->control = control;
	if (axis == 'X')
	{
		setInstance(0);
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS1_CONFIG, ADR_AXIS1_MAX_SPEED, ADR_AXIS1_MAX_ACCEL,
										   ADR_AXIS1_ENDSTOP, ADR_AXIS1_POWER, ADR_AXIS1_DEGREES,ADR_AXIS1_EFFECTS1,
										   ADR_AXIS1_SPEED_FILTER, ADR_AXIS1_ACCEL_FILTER, ADR_AXIS1_SCALER_SPEED, ADR_AXIS1_SCALER_ACCEL});
	}
	else if (axis == 'Y')
	{
		setInstance(1);
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS2_CONFIG, ADR_AXIS2_MAX_SPEED, ADR_AXIS2_MAX_ACCEL,
										   ADR_AXIS2_ENDSTOP, ADR_AXIS2_POWER, ADR_AXIS2_DEGREES,ADR_AXIS2_EFFECTS1,
										   ADR_AXIS2_SPEED_FILTER, ADR_AXIS2_ACCEL_FILTER, ADR_AXIS2_SCALER_SPEED, ADR_AXIS2_SCALER_ACCEL});
	}
	else if (axis == 'Z')
	{
		setInstance(2);
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS3_CONFIG, ADR_AXIS3_MAX_SPEED, ADR_AXIS3_MAX_ACCEL,
										   ADR_AXIS3_ENDSTOP, ADR_AXIS3_POWER, ADR_AXIS3_DEGREES,ADR_AXIS3_EFFECTS1,
										   ADR_AXIS3_SPEED_FILTER, ADR_AXIS3_ACCEL_FILTER, ADR_AXIS3_SCALER_SPEED, ADR_AXIS3_SCALER_ACCEL});
	}


	restoreFlash(); // Load parameters
	CommandHandler::registerCommands(); // Internal commands
	registerCommands();

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
	registerCommand("enctype", Axis_commands::enctype, "Encoder type get/set/list",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("drvtype", Axis_commands::drvtype, "Motor driver type get/set/list",CMDFLAG_GET | CMDFLAG_SET | CMDFLAG_INFOSTRING);
	registerCommand("pos", Axis_commands::pos, "Encoder position",CMDFLAG_GET);
	registerCommand("maxspeed", Axis_commands::maxspeed, "Speed limit in deg/s",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("maxtorquerate", Axis_commands::maxtorquerate, "Torque rate limit in counts/ms",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("fxratio", Axis_commands::fxratio, "Effect ratio. Reduces effects excluding endstop. 255=100%",CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("curtorque", Axis_commands::curtorque, "Axis torque",CMDFLAG_GET);
	registerCommand("curpos", Axis_commands::curpos, "Axis position",CMDFLAG_GET);

	registerCommand("filterSpeed_freq", Axis_commands::filterSpeed_freq, "Biquad filter frequency for speed", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("filterSpeed_q", Axis_commands::filterSpeed_q, "Biquad filter q for speed", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("filterAccel_freq", Axis_commands::filterAccel_freq, "Biquad filter frequency for accel", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("filterAccel_q", Axis_commands::filterAccel_q, "Biquad filter frequency for accel", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("scaleSpeed", Axis_commands::scaleSpeed, "Scale to map SPEED on full range", CMDFLAG_GET | CMDFLAG_SET);
	registerCommand("scaleAccel", Axis_commands::scaleAccel, "Scale to map ACCEL on full range", CMDFLAG_GET | CMDFLAG_SET);
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
		setDamperStrength((effects >> 8) & 0xff);
	}

	uint16_t filterStorage;
	if (Flash_Read(flashAddrs.speedFilter, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		filterSpeedCst.freq = freq;
		filterSpeedCst.q = q;
		speedFilter.setFc(filterSpeedCst.freq / filter_f);
		speedFilter.setQ(filterSpeedCst.q / 100.0);
	}

	if (Flash_Read(flashAddrs.accelFilter, &filterStorage))
	{
		uint32_t freq = filterStorage & 0x1FF;
		uint8_t q = (filterStorage >> 9) & 0x7F;
		filterAccelCst.freq = freq;
		filterAccelCst.q = q;
		accelFilter.setFc(filterAccelCst.freq / filter_f);
		accelFilter.setQ(filterAccelCst.q / 100.0);
	}

	uint16_t scaler;
	if (Flash_Read(flashAddrs.speedScale, &scaler))
	{
		this->scaleSpeed = scaler;
	}

	if (Flash_Read(flashAddrs.accelScale, &scaler))
	{
		this->scaleAccel = scaler;
	}


}
// Saves parameters to flash.
void Axis::saveFlash(){
	//NormalizedAxis::saveFlash();
	Flash_Write(flashAddrs.config, Axis::encodeConfToInt(this->conf));
//	Flash_Write(flashAddrs.maxSpeed, this->maxSpeedDegS);
//	Flash_Write(flashAddrs.maxAccel, (uint16_t)(this->maxTorqueRateMS));

	Flash_Write(flashAddrs.endstop, fx_ratio_i | (endstopStrength << 8));
	Flash_Write(flashAddrs.power, power);
	Flash_Write(flashAddrs.degrees, (degreesOfRotation & 0x7fff) | (invertAxis << 15));
	Flash_Write(flashAddrs.effects1, idlespringstrength | (damperIntensity << 8));

	uint16_t filterStorage;

	// save CF biquad
	filterStorage = (uint16_t)filterSpeedCst.freq & 0x1FF;
	filterStorage |= ( (uint16_t)filterSpeedCst.q & 0x7F ) << 9 ;
	Flash_Write(flashAddrs.speedFilter, filterStorage);

	filterStorage = (uint16_t)filterAccelCst.freq & 0x1FF;
	filterStorage |= ( (uint16_t)filterAccelCst.q & 0x7F ) << 9 ;
	Flash_Write(flashAddrs.accelFilter, filterStorage);

	// save scaler
	Flash_Write(flashAddrs.speedScale, (uint16_t) (this->scaleSpeed));
	Flash_Write(flashAddrs.accelScale, (uint16_t)(this->scaleAccel));

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

	//if (!drv->motorReady()) return;

	float angle = getEncAngle(this->drv->getEncoder());

	// Scale encoder value to set rotation range
	// Update a change of range only when new range is within valid range
	// if degree change, compute the SpeedScaler, it depends on degreesOfRotation
	if (nextDegreesOfRotation != degreesOfRotation && abs(scaleEncValue(angle, nextDegreesOfRotation)) < 0x7fff){
		degreesOfRotation = nextDegreesOfRotation;
	}


	// scaledEnc now gets inverted if necessary in updateMetrics
	int32_t scaledEnc = scaleEncValue(angle, degreesOfRotation);

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

	this->updateMetrics(angle);

}

void Axis::errorCallback(Error &error, bool cleared){
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

	// Pass encoder to driver again
	if(!this->drv->hasIntegratedEncoder()){
		this->drv->setEncoder(this->enc);
	}

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
//	drv->setAxis(axis);
	drv->restoreFlash();
	tmclimits.pid_torque_flux = getPower();
	drv->setLimits(tmclimits);
	//drv->setBiquadFlux(fluxbq);
	drv->setExternalEncoderAllowed(true);
	

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
	}else{
		this->conf.enctype = 0; // None encoder
	}

	float angle = getEncAngle(this->drv->getEncoder());
	//int32_t scaledEnc = scaleEncValue(angle, degreesOfRotation);
	// reset metrics
	this->resetMetrics(angle);

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
	// Reset filters
	speedFilter.calcBiquad();
	accelFilter.calcBiquad();
}


void Axis::updateMetrics(float new_pos) { // pos is degrees
	// store old value for next metric's computing
	metric.previous = metric.current;

	metric.current.posDegrees = new_pos;
	int32_t scaled_pos = scaleEncValue(new_pos, degreesOfRotation);
	metric.current.pos = scaled_pos;

	metric.current.speedInstant = (new_pos - metric.previous.posDegrees) * 1000.0; // deg/s
	metric.current.speed = speedFilter.process(metric.current.speedInstant * scaleSpeed);

	metric.current.accelInstant = metric.current.speedInstant - metric.previous.speedInstant;
	metric.current.accel = accelFilter.process(metric.current.accelInstant  * scaleAccel);

	metric.current.torque = 0;
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
	addtorque *= (float)endstopStrength * endstopGain * torqueScaler; // Apply endstop gain for stiffness. 0.15f
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
//	if(torque - metric.previous.torque)
	if(outOfBounds){
		torque = 0;
	}

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


CommandStatus Axis::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){

	switch(static_cast<Axis_commands>(cmd.cmdId)){

	case Axis_commands::power:
		handleGetSetFunc(cmd, replies, power, &Axis::setPower,this);
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
			replies.push_back(CommandReply(invertAxis ? 1 : 0));
		}
		else if (cmd.type == CMDtype::set)
		{
			invertAxis = cmd.val >= 1 ? true : false;
			resetMetrics(-metric.current.posDegrees);
		}
		break;

	case Axis_commands::idlespring:
		handleGetSetFunc(cmd, replies, idlespringstrength, &Axis::setIdleSpringStrength,this);
		break;

	case Axis_commands::axisdamper:
		handleGetSetFunc(cmd, replies, damperIntensity, &Axis::setDamperStrength,this);
		break;

	case Axis_commands::enctype:
		if(cmd.type == CMDtype::info){
			enc_chooser.replyAvailableClasses(replies,this->getEncType());
		}else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->getEncType()));
		}else if(cmd.type == CMDtype::set){
			this->setEncType(cmd.val);
		}
		break;

	case Axis_commands::drvtype:
		if(cmd.type == CMDtype::info){
			drv_chooser.replyAvailableClasses(replies);
		}
		else if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->getDrvType()));
		}else if(cmd.type == CMDtype::set){
			this->setDrvType(cmd.val);
		}
		break;

	case Axis_commands::pos:
		if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(this->drv->getEncoder()->getPos()));
		}
		else if (cmd.type == CMDtype::set && this->drv->getEncoder() != nullptr)
		{
			this->drv->getEncoder()->setPos(cmd.val);
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
			replies.push_back(CommandReply(this->fx_ratio_i));
		}else if(cmd.type == CMDtype::set){
			setFxRatio(cmd.val);
		}
		break;

	case Axis_commands::curpos:
		replies.push_back(CommandReply(this->metric.current.pos));
		break;
	case Axis_commands::curtorque:
		replies.push_back(CommandReply(this->metric.current.torque));
		break;

	case Axis_commands::scaleSpeed:
		if(cmd.type == CMDtype::info){
			replies.push_back(CommandReply("scale:1"));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(this->scaleSpeed));
		}
		else if (cmd.type == CMDtype::set)
		{
			uint32_t value = clip<uint32_t, uint32_t>(cmd.val, 1, 10000);
			this->scaleSpeed = value;
		}
		break;
	case Axis_commands::scaleAccel:
		if(cmd.type == CMDtype::info){
			replies.push_back(CommandReply("scale:0.1"));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(this->scaleAccel));
		}
		else if (cmd.type == CMDtype::set)
		{
			uint32_t value = clip<uint32_t, uint32_t>(cmd.val, 1, 10000);
			this->scaleAccel = value;
		}
		break;

	case Axis_commands::filterSpeed_freq:
		if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(this->filterSpeedCst.freq));
		}
		else if (cmd.type == CMDtype::set)
		{
			uint32_t value = clip<uint32_t, uint32_t>(cmd.val, 0, 500);
			this->filterSpeedCst.freq = value;
			speedFilter.setFc(filterSpeedCst.freq / filter_f);
		}
		break;
	case Axis_commands::filterSpeed_q:
		if(cmd.type == CMDtype::info){
			replies.push_back(CommandReply("scale:0.01"));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(this->filterSpeedCst.q));
		}
		else if (cmd.type == CMDtype::set)
		{
			uint32_t value = clip<uint32_t, uint32_t>(cmd.val, 0, 120);
			this->filterSpeedCst.q = value;
			speedFilter.setQ(filterSpeedCst.q / 100.0);
		}
		break;
	case Axis_commands::filterAccel_freq:
		if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(this->filterAccelCst.freq));
		}
		else if (cmd.type == CMDtype::set)
		{
			uint32_t value = clip<uint32_t, uint32_t>(cmd.val, 0, 500);
			this->filterAccelCst.freq = value;
			accelFilter.setFc(filterAccelCst.freq / filter_f);
		}
		break;
	case Axis_commands::filterAccel_q:
		if(cmd.type == CMDtype::info){
			replies.push_back(CommandReply("scale:0.01"));
		}
		else if (cmd.type == CMDtype::get)
		{
			replies.push_back(CommandReply(this->filterAccelCst.q));
		}
		else if (cmd.type == CMDtype::set)
		{
			uint32_t value = clip<uint32_t, uint32_t>(cmd.val, 0, 120);
			this->filterAccelCst.q = value;
			accelFilter.setQ(filterAccelCst.q / 100.0);
		}
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
