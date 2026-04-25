/*
 * Axis.h
 *
 *  Created on: 21.01.2021
 *      Author: Yannick / Lidders / Vincent
 * 
 *  Release 27.10.25: Vincent, add reconstruction filter, equalizer, slew rate.
 */

#ifndef SRC_AXIS_H_
#define SRC_AXIS_H_
#include <FFBoardMain.h>
#include <MotorPWM.h>
#include "usb_hid_ffb_desc.h"
#include "PersistentStorage.h"
#include "ButtonSource.h"
#include "EncoderLocal.h"
#include "LocalAnalog.h"
#include "AnalogSource.h"

#include "HidFFB.h"
#include "ffb_defs.h"
#include "TimerHandler.h"
#include "ClassChooser.h"
#include "ExtiHandler.h"
#include "EffectsCalculator.h"

#ifdef USE_DSP_FUNCTIONS
#include "arm_math.h"
#endif

#define INTERNAL_AXIS_DAMPER_SCALER 0.7
#define INTERNAL_AXIS_FRICTION_SCALER 0.7
#define INTERNAL_AXIS_INERTIA_SCALER 0.7
#ifndef AXIS_SPEEDLIMITER_P
#define AXIS_SPEEDLIMITER_P 0.3
#endif
#ifndef AXIS_SPEEDLIMITER_I
#define AXIS_SPEEDLIMITER_I 0.03
#endif

/**
 * @brief Global control flags for all axes.
 */
struct Control_t {
	bool emergency = false;				//!< Emergency stop is active.
	bool usb_disabled = true;			//!< FFB is disabled by USB.
	bool update_disabled = true;		//!< FFB updates are disabled.
	bool request_update_disabled = false; //!< A request to disable FFB updates is pending.
//	bool usb_update_flag = false;
//	bool update_flag = false;
	bool resetEncoder = false;			//!< A request to reset the encoder is pending.
};

/**
 * @brief Defines the flash memory addresses for axis-specific settings.
 */
struct AxisFlashAddresses
{
	uint16_t config = ADR_AXIS1_CONFIG;
	uint16_t maxSpeed = ADR_AXIS1_MAX_SPEED;
	uint16_t maxAccel = ADR_AXIS1_MAX_ACCEL;
	uint16_t maxSlewRateDrv = ADR_AXIS1_MAX_SLEWRATE_DRV;

	uint16_t endstop = ADR_AXIS1_ENDSTOP;
	uint16_t power = ADR_AXIS1_POWER;
	uint16_t degrees = ADR_AXIS1_DEGREES;
	uint16_t effects1 = ADR_AXIS1_EFFECTS1;
	uint16_t effects2 = ADR_AXIS1_EFFECTS2;
	uint16_t encoderRatio = ADR_AXIS1_ENC_RATIO;

	uint16_t speedAccelFilter = ADR_AXIS1_SPEEDACCEL_FILTER;
	uint16_t postprocess1 = ADR_AXIS1_POSTPROCESS1;
};

/**
 * @brief Configuration for an axis, including driver and encoder types.
 */
struct AxisConfig
{
	uint8_t drvtype = 0; //!< Motor driver type ID.
	uint8_t enctype = 0; //!< Encoder type ID.
};
/**
 * @brief Holds the physical metrics of an axis at a point in time.
 */
struct metric_t {
	float accel = 0;	  //!< Acceleration in deg/s².
	float speed = 0;	  //!< Speed in deg/s.
	int32_t pos_scaled_16b = 0;	  //!< Scaled position as a 16-bit integer (-0x7fff to 0x7fff).
	float pos_f = 0;	  //!< Scaled position as a float (-1.0 to 1.0).
	float posDegrees = 0; //!< Position in degrees, not scaled to the selected range.
	int32_t torque = 0;	  //!< Total torque applied to the axis.
};


/**
 * @brief Holds the current and previous metrics for an axis, used for calculating derivatives.
 */
struct axis_metric_t {
	metric_t current;  //!< Current metrics.
	metric_t previous; //!< Metrics from the previous update cycle.
};

/**
 * @brief Represents a gear ratio for scaling encoder values.
 */
struct GearRatio_t{
	uint8_t denominator = 0; //!< Denominator of the gear ratio.
	uint8_t numerator = 0;   //!< Numerator of the gear ratio.
	float gearRatio = 1.0;   //!< The calculated gear ratio (numerator/denominator).
};

enum class Axis_commands : uint32_t{
	power=0x00,degrees=0x01,esgain,zeroenc,invert,idlespring,axisdamper,enctype,drvtype,
	pos,curtorque,curpos,curspd,curaccel,
	fxratio,reductionScaler,
	filterSpeed, filterAccel, filterProfileId,cpr,axisfriction,axisinertia,
	maxspeed,slewrate,
	calibrate_maxSlewRateDrv,
	maxSlewRateDrv,
	expo,exposcale
};

/**
 * @brief This class represents a single FFB axis.
 * It handles the motor driver, encoder, and all related calculations for force feedback effects.
 */
class Axis : public PersistentStorage, public CommandHandler, public ErrorHandler
{
public:
	/**
	 * @brief Construct a new Axis object.
	 * @param axis The character identifier for this axis (e.g., 'x', 'y').
	 * @param control A pointer to the global control structure.
	 */
	Axis(char axis, volatile Control_t* control);
	virtual ~Axis();

	static ClassIdentifier info; //!< Static class identifier.
	const ClassIdentifier getInfo();
	const ClassType getClassType() override {return ClassType::Axis;};

	virtual std::string getHelpstring() { return "FFB axis"	;}
	// Dynamic classes
	/**
	 * @brief Sets the motor driver type.
	 * @param drvtype The type of the motor driver.
	 */
	void setDrvType(uint8_t drvtype);

	/**
	 * @brief Sets the encoder type.
	 * @param enctype The type of the encoder.
	 */
	void setEncType(uint8_t enctype);

	/**
	 * @brief Gets the motor driver type.
	 * @return The type of the motor driver.
	 */
	uint8_t getDrvType();

	/**
	 * @brief Gets the encoder type.
	 * @return The type of the encoder.
	 */
	uint8_t getEncType();

	/**
	 * @brief Gets the encoder instance.
	 * @return A pointer to the encoder instance.
	 */
	Encoder* getEncoder();

	/**
	 * @brief Gets the motor driver instance.
	 * @return A pointer to the motor driver instance.
	 */
	MotorDriver* getDriver();

	/**
	 * @brief Called on USB disconnect and suspend.
	 */
	void usbSuspend();

	/**
	 * @brief Called on USB resume. Enables the motor driver.
	 */
	void usbResume();

	/**
	 * @brief Saves axis settings to flash memory.
	 * @override from PersistentStorage
	 */
	void saveFlash() override;

	/**
	 * @brief Restores axis settings from flash memory.
	 * @override from PersistentStorage
	 */
	void restoreFlash() override;

	/**
	 * @brief Prepares the axis for an update cycle. Called from the main loop before effects are calculated.
	 * Reads the encoder, scales the value, and updates metrics (speed, acceleration).
	 */
	void prepareForUpdate();

	/**
	 * @brief Sends the final calculated torque to the motor driver.
	 */
	void updateDriveTorque();

	/**
	 * @brief Triggers an emergency stop. Disables the motor driver.
	 * @param reset If true, resets the axis after stopping.
	 */
	void emergencyStop(bool reset);

	/**
	 * @brief Sets the position of the axis.
	 * @param val The new position value.
	 */
	void setPos(uint16_t val);

	/**
	 * @brief Zeros the current encoder position.
	 */
	void zeroPos();

	/**
	 * @brief Checks if FFB is globally active.
	 * @return true if FFB is active, false otherwise.
	 */
	bool getFfbActive();

	/**
	 * @brief Scales an encoder value.
	 * @param angle The angle to scale.
	 * @param degrees The total degrees of rotation.
	 * @return A pair containing the scaled integer value and the float value.
	 */
	std::pair<int32_t,float> scaleEncValue(float angle, uint16_t degrees);

	/**
	 * @brief Gets the angle from the encoder.
	 * @param enc A pointer to the encoder.
	 * @return The angle in degrees.
	 */
	float 	getEncAngle(Encoder *enc);

	/**
	 * @brief Sets the maximum power (torque) of the motor.
	 * @param power The new power value.
	 */
	void setPower(uint16_t power);

	/**
	 * @brief Callback for handling errors.
	 * @param error The error that occurred.
	 * @param cleared Whether the error has been cleared.
	 * @override from ErrorHandler
	 */
	void errorCallback(const Error &error, bool cleared) override;

	/**
	 * @brief Registers the commands for this axis with the command handler.
	 */
	void registerCommands();
	/**
	 * @brief Handles command line interface commands for this axis.
	 * @param cmd The parsed command.
	 * @param replies A vector of replies to be sent back.
	 * @return The status of the command execution.
	 * @override from CommandHandler
	 */
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;

	ClassChooser<MotorDriver> driverChooser; //!< Class chooser for motor drivers.
	ClassChooser<Encoder> encoderChooser;     //!< Class chooser for encoders.

	/**
	 * @brief Gets the last scaled encoder value.
	 * @return The last scaled encoder value.
	 */
	int32_t getLastScaledEnc();

	/**
	 * @brief Resets the metrics of the axis.
	 * @param new_pos The new position to reset to.
	 */
	void resetMetrics(float new_pos);

	/**
	 * @brief Updates the metrics of the axis.
	 * @param new_pos The new position.
	 */
	void updateMetrics(float new_pos);

	/**
	 * @brief Updates the idle spring force.
	 * @return The calculated idle spring force.
	 */
	int32_t updateIdleSpringForce();

	/**
	 * @brief Sets the idle spring strength.
	 * @param spring The new spring strength.
	 */
	void setIdleSpringStrength(uint8_t spring);

	/**
	 * @brief Sets the strength and filter for an effect.
	 * @param val The new strength value.
	 * @param valToSet A reference to the value to be set.
	 * @param filter A reference to the biquad filter.
	 */
	void setFxStrengthAndFilter(uint8_t val,uint8_t& valToSet, Biquad& filter);

	/**
	 * @brief Calculates the mechanical effects (damper, friction, inertia) that are always active.
	 * These are separate from the HID FFB effects sent by the game.
	 * @param ffb_on A flag indicating if FFB is on.
	 */
	void calculateMechanicalEffects(bool ffb_on);

	/**
	 * @brief Gets the current torque.
	 * @return The current torque scaled as a 32-bit signed value.
	 */
	int32_t getTorque();

	/**
	 * @brief Calculates the endstop torque.
	 * @return The calculated endstop torque.
	 */
	int32_t calculateEndstopTorque();

	/**
	 * @brief Calculates the FFB torque exponential torque, from the input torque and apply expo and scaler
	 * @return The calculated exponential torque.
	 */
	int64_t calculateFFBTorque();

	/**
	 * @brief Starts a force fade-in.
	 * @param start The starting force multiplier.
	 * @param fadeTime The duration of the fade-in.
	 */
	void startForceFadeIn(float start = 0,float fadeTime = 0.5);

	metric_t* getMetrics();

	/**
	 * @brief Sets the FFB effect torque.
	 * @param torque The new FFB effect torque from the EffectsCalculator.
	 */
	void setFfbEffectTorque(int64_t torque);

	/**
	 * @brief Updates the total torque.
	 * @param totalTorque A pointer to the total torque value.
	 * @return true if the torque was updated, false otherwise.
	 */
	bool updateTorque(int32_t* totalTorque);


	void updateSamplerate(float newSamplerate);
	void updateFilters(uint8_t profileId);

	/**
	 * @brief Sets the gear ratio.
	 * @param numerator The numerator of the gear ratio.
	 * @param denominator The denominator of the gear ratio.
	 */
	void setGearRatio(uint8_t numerator,uint8_t denominator);

	static const std::vector<class_entry<MotorDriver>> axis1_drivers; //!< List of available motor drivers for the first axis.
	static const std::vector<class_entry<MotorDriver>> axis2_drivers; //!< List of available motor drivers for the second axis.

private:
	// Internal constants
	const float AXIS_DAMPER_RATIO = INTERNAL_SCALER_DAMPER * INTERNAL_AXIS_DAMPER_SCALER / 255.0;
	const float AXIS_INERTIA_RATIO = INTERNAL_SCALER_INERTIA * INTERNAL_AXIS_INERTIA_SCALER / 255.0;

	// Private methods
	/**
	 * @brief Sets the degrees of rotation for the axis.
	 * @param degrees The new range of rotation.
	 */
	void setDegrees(uint16_t degrees);
	/**
	 * @brief Returns the current power setting of the axis.
	 * @return The power value.
	 */
	uint16_t getPower();
	/**
	 * @brief Returns the calculated torque scaler.
	 * @return The torque scaler value.
	 */
	bool isInverted();
	/**
	 * @brief Sets the ratio between game effects and endstop force and updates the internal torque scaler based on power and fx_ratio
	 * @param val The new ratio value (0-255).
	 */
	void setEffectRatio(uint8_t val);
	/**
	 * @brief Sets the exponential torque curve.
	 * @param val The new expo value.
	 */
	void setExpo(int val);

	int32_t calculateExpoTorque(int32_t torque);
	/**
	 * @brief Applies the speed limiter PI controller to the torque.
	 * @param torque A reference to the torque value to be modified.
	 * @return torque update to apply to reduced de speed.
	 */
	int64_t applySpeedLimiterTorque(int64_t& torque);
	/**
	 * @brief Applies the torque slew rate limiter to the torque.
	 * @param torque A reference to the torque value to be modified.
	 */
	void applyTorqueSlewRateLimiter(int64_t& torque);
	/**
	 * @brief Decodes the axis configuration from a 16-bit integer stored in flash.
	 * @param val The 16-bit encoded configuration value.
	 */
	static AxisConfig decodeConfFromInt(uint16_t val);
	/**
	 * @brief Encodes the axis configuration into a 16-bit integer for flash storage.
	 * @param conf The AxisConfig struct to encode.
	 * @return The encoded 16-bit value.
	 */
	static uint16_t encodeConfToInt(AxisConfig conf);



	// Member variables
	AxisFlashAddresses flashAddresses;			//!< Flash memory addresses for this axis.
	volatile Control_t* control;		//!< Pointer to the global control structure.
	AxisConfig conf;					//!< Configuration for this axis (driver and encoder types).
	char axis;							//!< Axis identifier ('X', 'Y', 'Z').

	std::unique_ptr<MotorDriver> drv = std::make_unique<MotorDriver>(); //!< Unique pointer to the active motor driver.
	std::shared_ptr<Encoder> enc = nullptr;	//!< Shared pointer to the active encoder.

	bool outOfBounds = false;			//!< Flag indicating if the axis is out of its valid range.
	const Error outOfBoundsError = Error(ErrorCode::axisOutOfRange,ErrorType::warning,"Axis out of bounds"); //!< Error object for out-of-bounds condition.

	// Force fade-in effect
	float forceFadeDuration = 1.0;			//!< Duration of the force fade-in in seconds.
	float forceFadeMultiplier = 1.0;		//!< Current multiplier for the force fade-in.

	float encoderOffset = 0; //!< Offset for absolute encoders.
	uint16_t degreesOfRotation = 900;		//!< Current degrees of rotation.
	uint16_t previousDegreesOfRotation = degreesOfRotation; //!< Previous degrees of rotation (for smooth transitions).
	uint16_t nextDegreesOfRotation = degreesOfRotation; //!< Target degrees of rotation.

	// Limiters
	uint16_t maxSlewRate_Driver = MAX_SLEW_RATE;		//!< Maximum slew rate as measured by the driver (in units/ms).
	uint16_t maxSpeedDegS  = 0;		//!< Maximum speed in degrees per second. 0 to disable.
	uint32_t maxTorqueRateMS = 0;		//!< Maximum torque rate of change per millisecond. 0 to disable.

	float speedLimiterP = AXIS_SPEEDLIMITER_P; //!< Proportional term for the speed limiter.
	float speedLimiterI = AXIS_SPEEDLIMITER_I; //!< Integral term for the speed limiter.
#ifdef USE_DSP_FUNCTIONS
	arm_pid_instance_f32 speedLimiterPID; //!< PID instance for the speed limiter.
#else
	// Speed limiter PID
	float speedLimitReducerI = 0;
#endif

	// Axis metrics
	axis_metric_t metric;			//!< Current and previous physical metrics of the axis.
	float previousFrameSpeed = 0;			//!< Instantaneous speed from the last cycle, used for acceleration calculation.

	// Torque components
	int64_t ffbEffectTorque = 0;		//!< Torque from HID FFB effects.
	int32_t mechanicalEffectTorque = 0;	//!< Torque from mechanical effects (damper, friction, inertia).

	// Power and scaling
	uint16_t power = 5000;			//!< Maximum motor power/torque.
	uint8_t effectRatio = 204;		//!< Ratio of HID effects vs. endstop effects (0-255).
	float effectRatioScaler = 0;	//!< Scaler for HID effects based on effectRatio.
	float torqueScaler = 0;			//!< Final torque scaler based on power.

	// Axis configuration
	bool invertAxis = true;			//!< Invert axis direction.
	uint8_t endstopStrength = 127;	//!< Stiffness of the endstop effect.
	const float endstopGain = 25;	//!< Overall maximum endstop intensity.

	// Idle spring effect
	uint8_t idleSpringStrength = 127; //!< Strength of the idle spring.
	int16_t idleSpringClip = 0;       //!< Maximum force for the idle spring.
	float idleSpringScale = 0;        //!< Scaler for the idle spring force.
	bool motorWasNotReady = true;     //!< Flag to detect motor readiness transition.

	// Slew rate calibration tracking: true when Axis requested a calibration and
	// is waiting for the driver to finish measuring the max slew rate.
	bool awaitingSlewCalibration = false;

	// Filters
	// TODO tune these and check if it is really stable and beneficial to the FFB. index 4 placeholder
	const std::array<biquad_constant_t,4> filterSpeedCst = { {{ 40, 55 }, { 70, 55 }, { 120, 55 }, {180, 55}} }; //!< Speed filter profiles.
	const std::array<biquad_constant_t,4> filterAccelCst = { {{ 40, 30 }, { 55, 30 }, { 70, 30 }, {120, 55}} }; //!< Acceleration filter profiles.
	const biquad_constant_t filterDamperCst = {60, 55};     //!< Damper filter constants.
	const biquad_constant_t filterFrictionCst = {50, 20};   //!< Friction filter constants.
	const biquad_constant_t filterInertiaCst = {20, 20};    //!< Inertia filter constants.
	uint8_t filterProfileId = 1; //!< Currently selected filter profile ID.
	float filter_f = 1000.0; // 1khz
	const int32_t internalFxClip = 20000; //!< Clipping value for internal effects.

	// Internal effects intensity
	uint8_t damperIntensity = 30;	//!< Intensity of the internal damper effect.
	uint8_t frictionIntensity = 0;	//!< Intensity of the internal friction effect.
	uint8_t inertiaIntensity = 0;	//!< Intensity of the internal inertia effect.

	// Biquad filter instances
	Biquad speedFilter = Biquad(BiquadType::lowpass, filterSpeedCst[filterProfileId].freq/filter_f, filterSpeedCst[filterProfileId].q/100.0, 0.0);
	Biquad accelFilter = Biquad(BiquadType::lowpass, filterAccelCst[filterProfileId].freq/filter_f, filterAccelCst[filterProfileId].q/100.0, 0.0);
	Biquad damperFilter = Biquad(BiquadType::lowpass, filterDamperCst.freq/filter_f, filterDamperCst.q / 100.0, 0.0);
	Biquad frictionFilter = Biquad(BiquadType::lowpass, filterFrictionCst.freq/filter_f, filterFrictionCst.q / 100.0, 0.0);
	Biquad inertiaFilter = Biquad(BiquadType::lowpass, filterInertiaCst.freq/filter_f, filterInertiaCst.q / 100.0, 0.0);

	// Post-processing
	GearRatio_t gearRatio;	//!< Gear ratio between encoder and axis.
	int expoValue = 0;		//!< Raw integer value for the expo curve. Formula: v = val*2 => v<0 ? 1/-v : v
	float expo = 1;			//!< Calculated exponent for the torque curve.
	float expoScaler = 50;	//!< Scaler for the expo calculation : 0.28 to 3.54
};

#endif /* SRC_AXIS_H_ */
