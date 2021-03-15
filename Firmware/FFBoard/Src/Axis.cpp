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
 * Sources for class choosers here
 */

// 0-63 valid ids
const std::vector<class_entry<MotorDriver> > motor_sources =
	{
		add_class<MotorDriver, MotorDriver>(),
#ifdef TMC4671DRIVER
		add_class<TMC4671, MotorDriver>(),
#endif
#ifdef PWMDRIVER
		add_class<MotorPWM, MotorDriver>(),
#endif
};

// 0-63 valid ids
std::vector<class_entry<Encoder> > encoder_sources =
	{
		add_class<Encoder, Encoder>(),
#ifdef LOCALENCODER
		add_class<EncoderLocal, Encoder>(),
#endif
};



// TODO class type for parser? (Simhub for example)
//////////////////////////////////////////////

Axis::Axis(char axis,volatile Control_t* control) : NormalizedAxis(axis), drv_chooser(motor_sources), enc_chooser(encoder_sources)
{
	// Create HID FFB handler. Will receive all usb messages directly
//	this->axis = axis;
	this->control = control;
	if (axis == 'X')
	{
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS1_CONFIG, ADR_TMC1_CPR});
	}
	else if (axis == 'Y')
	{
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS2_CONFIG, ADR_TMC2_CPR});
	}
	else if (axis == 'Z')
	{
		this->flashAddrs = AxisFlashAddrs({ADR_AXIS3_CONFIG, ADR_TMC3_CPR});
	}

	restoreFlash(); // Load parameters
}

Axis::~Axis()
{
	delete drv;
	delete enc;
}

/*
 * Read parameters from flash and restore settings
 */
void Axis::restoreFlash(){
	NormalizedAxis::restoreFlash();
	// read all constants
	uint16_t confint;
	if (Flash_Read(flashAddrs.config, &confint)){
		this->conf = Axis::decodeConfFromInt(confint);
	}else{
		pulseErrLed();
	}

	setDrvType(this->conf.drvtype);
	setEncType(this->conf.enctype);

	uint16_t cpr = 0;
	if (Flash_Read(flashAddrs.cpr, &cpr)){
		this->setCpr(cpr);
	}else{
		pulseErrLed();
	}
}
// Saves parameters to flash
void Axis::saveFlash(){
	NormalizedAxis::saveFlash();
	Flash_Write(flashAddrs.config, Axis::encodeConfToInt(this->conf));
	if (hasEnc()){
		Flash_Write(flashAddrs.cpr, enc->getCpr());
	}
	drv->saveFlash(); // Motor driver
}



uint8_t Axis::getDrvType(){
	return (uint8_t)this->conf.drvtype;
}
uint8_t Axis::getEncType(){
	return (uint8_t)this->conf.enctype;
}

bool Axis::hasEnc(){
	return (this->enc != nullptr);
}

void Axis::setCpr(uint16_t val){
	if (hasEnc()){
		this->enc->setCpr(val);
	}
}

void Axis::setPos(uint16_t val)
{
	if (hasEnc())
	{
		this->enc->setPos(val);
		int32_t scaledEnc = getEncValue(enc, degreesOfRotation);
		this->resetMetrics(scaledEnc);
	}
}

/*
 * Called from FFBWheel->Update() via AxesManager->Update()
 */
void Axis::prepareForUpdate(){
	if (drv == nullptr || enc == nullptr){
		pulseErrLed();
		return;
	}

	if (!drv->motorReady()) return;

	// Scale encoder value to set rotation range
	// Update a change of range only when new range is within valid range
	if (nextDegreesOfRotation != degreesOfRotation && abs(getEncValue(enc, nextDegreesOfRotation)) < 0x7fff){
		degreesOfRotation = nextDegreesOfRotation;
	}
	// scaledEnc now gets inverted if necessary in updateMetrics
	int32_t scaledEnc = getEncValue(enc, degreesOfRotation);

	if (abs(scaledEnc) > 0xffff){
		// We are way off. Shut down
		drv->stopMotor();
		pulseErrLed();
	}

	this->updateMetrics(scaledEnc);

	if (this->conf.drvtype == TMC4671::info.id){
		TMC4671 *drv = static_cast<TMC4671 *>(this->drv);
		//drv->Run(); // TODO thread!
		if(drv->getState() == TMC_ControlState::HardError || drv->estopTriggered){
			emergencyStop();
		}
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
	NormalizedAxis::setPower(power);
	// Update hardware limits for TMC for safety
	if (this->conf.drvtype == TMC4671::info.id)
	{
		TMC4671 *drv = static_cast<TMC4671 *>(this->drv);
		//tmclimits.pid_uq_ud = power;
		//tmclimits.pid_torque_flux = power;
		drv->setTorqueLimit(power);
	}
}

// create and setup a motor driver
void Axis::setDrvType(uint8_t drvtype)
{
	//	extern USBD_HandleTypeDef hUsbDeviceFS;
	if (!drv_chooser.isValidClassId(drvtype))
	{
		return;
	}

	Encoder *drvenc = dynamic_cast<Encoder *>(drv); // Cast old driver to encoder
	if (drv != nullptr)
	{
		if (enc != nullptr && drvenc != nullptr)
		{
			for (auto it = encoder_sources.begin(); it != encoder_sources.end(); it++)
			{
				// Delete drv from encoder sources if present
				if (drvenc->getInfo().id == it->info.id)
				{
					encoder_sources.erase(it);
					setEncType(0); // reset encoder
					break;
				}
			}
		}
		delete drv;
		drv = nullptr;
	}

	this->drv = drv_chooser.Create((uint16_t)drvtype);
	if (this->drv == nullptr)
	{
		return;
	}
	this->conf.drvtype = drvtype;
	drvenc = dynamic_cast<Encoder *>(drv); // Cast new driver to encoder
	// Special handling for tmc4671.
	if (this->conf.drvtype == TMC4671::info.id)
	{
		setupTMC4671();
	}
	else
	{ // reset the tmc channel info if not a TMC drive
		this->conf.tmc_cs = 0;
	}

	// Add driver to encoder sources if also implements encoder

	if (drvenc != nullptr)
	{
		if (!enc_chooser.isValidClassId(drv->getInfo().id))
		{
			encoder_sources.push_back(make_class_entry<Encoder>(drv->getInfo(), drvenc));
			setEncType(drv->getInfo().id); // Auto preset driver as encoder
		}
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
	this->setupTMC4671ForAxis(axis);
}

void Axis::setupTMC4671ForAxis(char axis)
{
	TMC4671 *drv = static_cast<TMC4671 *>(this->drv);
	drv->setAddress(this->conf.tmc_cs, (uint8_t)(axis-'W')); // X=1, Y=2, etc
//	drv->initialize();
	drv->initialize();
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

void Axis::setEncType(uint8_t enctype)
{

	if (enc_chooser.isValidClassId(enctype))
	{
		if (enc != nullptr && enc->getInfo().id != enctype && enctype != drv->getInfo().id && enc->getInfo().id != drv->getInfo().id)
		{
			delete enc;
		}
		this->conf.enctype = (enctype);
		this->enc = enc_chooser.Create(enctype);
	}
	uint16_t cpr = 0;
	if (Flash_Read(flashAddrs.cpr, &cpr))
	{
		this->enc->setCpr(cpr);
	}
	int32_t scaledEnc = getEncValue(enc, degreesOfRotation);
	this->resetMetrics(scaledEnc);
}

/*
 * Returns a scaled encoder value between -0x7fff and 0x7fff with a range of degrees
 */
//TODO JL - use a precalc scaler to make this more efficient
int32_t Axis::getEncValue(Encoder *enc, uint16_t degrees){
	if (enc == nullptr || degrees == 0){
		return 0x7fff; // Return center if no encoder present
	}
	float angle = 360.0 * ((float)enc->getPos() / (float)enc->getCpr());
	int32_t val = (0xffff / (float)degrees) * angle;
	if (isInverted()){
		val= -val;
	}
	return val;
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

void Axis::processHidCommand(HID_Custom_Data_t *data){
	uint8_t axis = (data->cmd >> 6) & 0x3;
	if(axis != this->axis){
		return;
	}


	switch (data->cmd&0x3F){
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

		default:

		break;
	}
	return;
}


// { drvtype, power, zeroenc, enctype, cpr, pos, degrees
//, esgain, fxratio, idlespring, friction, invert,  };

ParseStatus Axis::command(ParsedCommand *cmd, std::string *reply)
{
	if (cmd->axis != axis){
		return ParseStatus::NOT_FOUND;
	}

	// Check the super class command handler first
	ParseStatus flag = NormalizedAxis::command(cmd, reply);
	if ( flag == ParseStatus::OK) {
		return flag;
	}

	flag = ParseStatus::OK;

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
			*reply += drv_chooser.printAvailableClasses();
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
		else if (cmd->type == CMDtype::set && enc_chooser.isValidClassId(cmd->val))
		{
			setEncType((cmd->val));
		}
		else
		{
			*reply += enc_chooser.printAvailableClasses();
		}
	}
	else if (cmd->cmd == "cpr")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(this->enc->getCpr());
		}
		else if (cmd->type == CMDtype::set && this->enc != nullptr)
		{
			this->enc->setCpr(cmd->val);
		}
		else
		{
			flag = ParseStatus::ERR;
			*reply += "Err. Setup enctype first";
		}
	}
	else if (cmd->cmd == "pos")
	{
		if (cmd->type == CMDtype::get)
		{
			*reply += std::to_string(this->enc->getPos());
		}
		else if (cmd->type == CMDtype::set && this->enc != nullptr)
		{
			this->enc->setPos(cmd->val);
		}
		else
		{
			flag = ParseStatus::ERR;
			*reply += "Err. Setup enctype first";
		}
	}
	else if (cmd->cmd == "tmc")
	{
		if (cmd->type == CMDtype::get)
		{
			if (this->getDrvType() == 1)
			{
				TMC4671 *drv = static_cast<TMC4671 *>(this->drv);
				*reply += std::to_string(drv->getCSchan());
			}
			else
			{
				flag = ParseStatus::ERR;
				*reply += "Err. Drive not TMC type";
			}
		}
		else if (cmd->type == CMDtype::set)
		{
			this->conf.tmc_cs = cmd->val & 0x3;
			if (this->getDrvType() == 1)
			{
				this->setupTMC4671();
			}
		}
		else
		{
			*reply += "Set the TMC Board channel for this axis.\ne.g. If you have one TMC board & want to use it for FFB on the Y axis - use: y.tmc=1 .";
		}
	}else{
		flag = ParseStatus::NOT_FOUND;
	}

	return flag;
};


/*
 * Helper functions for encoding and decoding flash variables
 */
AxisConfig Axis::decodeConfFromInt(uint16_t val)
{
	// 0-7 Enc 8-15 Mot
	AxisConfig conf;
	conf.enctype = ((val)&0x3f);
	conf.drvtype = ((val >> 6) & 0x3f);
	conf.tmc_cs = (val >> 12) & 0x3;
	return conf;
}
uint16_t Axis::encodeConfToInt(AxisConfig conf)
{
	uint16_t val = (uint8_t)conf.enctype & 0x3f;
	val |= ((uint8_t)conf.drvtype & 0x3f) << 6;
	val |= (conf.tmc_cs & 0x3) << 12;
	return val;
}
