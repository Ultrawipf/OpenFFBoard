/*
 * TMC4671.h
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#ifndef TMC4671_H_
#define TMC4671_H_
#include "constants.h"
#include <vector>
#include "cppmain.h"
#include "MotorDriver.h"
#include "Encoder.h"
#include "ChoosableClass.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include "SpiHandler.h"
#include "thread.hpp"
#include "ExtiHandler.h"
#include "SPI.h"
#include "TimerHandler.h"

#include "semaphore.hpp"
#include "OutputPin.h"
#include "cpp_target_config.h"

#define SPITIMEOUT 500
#define TMC_THREAD_MEM 256
#define TMC_THREAD_PRIO 25 // Must be higher than main thread

extern SPI_HandleTypeDef HSPIDRV;

#ifdef TIM_TMC
extern TIM_HandleTypeDef TIM_TMC;
#endif

enum class TMC_ControlState {uninitialized,waitPower,Shutdown,Running,ABN_init,AENC_init,HardError,OverTemp,EncoderFinished,IndexSearch,FullCalibration};

enum class TMC_PwmMode : uint8_t {off = 0,HSlow_LShigh = 1, HShigh_LSlow = 2, res2 = 3, res3 = 4, PWM_LS = 5, PWM_HS = 6, PWM_FOC = 7};


//enum class TMC_ControlState {uninitialized,No_power,Shutdown,Running,Init_wait,ABN_init,AENC_init,Enc_bang,HardError,OverTemp,EncoderFinished};
enum class ENC_InitState {startFullCalib,findIndex,uninitialized,estimating,aligning,checking,OK};
enum class TMC_StartupType{NONE,coldStart,warmStart};

enum class MotorType : uint8_t {NONE=0,DC=1,STEPPER=2,BLDC=3,ERR};
enum class PhiE : uint8_t {ext=1,openloop=2,abn=3,hall=5,aenc=6,aencE=7,NONE};
enum class MotionMode : uint8_t {stop=0,torque=1,velocity=2,position=3,prbsflux=4,prbstorque=5,prbsvelocity=6,uqudext=8,encminimove=9,NONE};
enum class FFMode : uint8_t {none=0,velocity=1,torque=2};
enum class PosSelection : uint8_t {PhiE=0, PhiE_ext=1, PhiE_openloop=2, PhiE_abn=3, res1=4, PhiE_hal=5, PhiE_aenc=6, PhiA_aenc=7, res2=8, PhiM_abn=9, PhiM_abn2=10, PhiM_aenc=11, PhiM_hal=12};
enum class VelSelection : uint8_t {PhiE=0, PhiE_ext=1, PhiE_openloop=2, PhiE_abn=3, res1=4, PhiE_hal=5, PhiE_aenc=6, PhiA_aenc=7, res2=8, PhiM_abn=9, PhiM_abn2=10, PhiM_aenc=11, PhiM_hal=12};
enum class EncoderType_TMC : uint8_t {NONE=0,abn=1,sincos=2,uvw=3,hall=4,ext=5}; // max 7

// Hardware versions for identifying different types
enum class TMC_HW_Ver : uint8_t {NONE=0,v1_0,v1_2,v1_2_2,v1_2_2_LEM20,v1_2_2_TMCS};
// Selectable version names to be listed in commands
const std::vector<std::pair<TMC_HW_Ver,std::string>> tmcHwVersionNames{
			std::make_pair(TMC_HW_Ver::NONE,"Undefined"), // Do not select. Default but disables some safety features
			std::make_pair(TMC_HW_Ver::v1_0,"v1.0"),
			std::make_pair(TMC_HW_Ver::v1_2,"v1.2 AD8417"),
			std::make_pair(TMC_HW_Ver::v1_2_2,"v1.2.2 LEM GO 10"),
			std::make_pair(TMC_HW_Ver::v1_2_2_LEM20,"v1.2.2 LEM GO 20"),
			std::make_pair(TMC_HW_Ver::v1_2_2_TMCS,"v1.2.2 TMCS1100A2")
};

struct TMC4671MotConf{
	MotorType motor_type = MotorType::NONE; //saved
	EncoderType_TMC enctype = EncoderType_TMC::NONE; //saved
	PhiE phiEsource 	= PhiE::ext;
	uint16_t pole_pairs = 4; //saved
	PosSelection pos_sel = PosSelection::PhiE;
	VelSelection vel_sel = VelSelection::PhiE;
};

/**
 * Settings that depend on the hardware version
 */
struct TMC4671HardwareTypeConf{
	TMC_HW_Ver hwVersion = TMC_HW_Ver::NONE;
	int adcOffset = 0;
	float thermistor_R2 = 1500;
	float thermistor_R = 22000;
	float thermistor_Beta = 4300;
	bool temperatureEnabled = false; // Enables temperature readings
	float temp_limit = 90;
	float currentScaler = 2.5 / (0x7fff * 60.0 * 0.0015); // Converts from adc counts to current in Amps
	uint16_t brakeLimLow = 50700;
	uint16_t brakeLimHigh = 50900;
	// Todo restrict allowed motor and encoder types
};


// Mapping of bits in status flag register and mask
union StatusFlags {
	struct StatusFlags_s {
		uint32_t pid_x_target_limit : 1,
		pid_x_target_ddt_limit : 1,
		pid_x_errsum_limit : 1,
		pid_x_output_limit : 1,
		pid_v_target_limit : 1,
		pid_v_target_ddt_limit : 1,
		pid_v_errsum_limit : 1,
		pid_v_output_limit : 1,
		pid_id_target_limit : 1,
		pid_id_target_ddt_limit : 1,
		pid_id_errsum_limit : 1,
		pid_id_output_limit : 1,
		pid_iq_target_limit : 1,
		pid_iq_target_ddt_limit : 1,
		pid_iq_errsum_limit : 1,
		pid_iq_output_limit : 1,
		ipark_cirlim_limit_u_d : 1,
		ipark_cirlim_limit_u_q : 1,
		ipark_cirlim_limit_u_r : 1,
		not_PLL_locked : 1,
		ref_sw_r : 1,
		ref_sw_h : 1,
		ref_sw_l : 1,
		res1:1,
		pwm_min : 1,
		pwm_max : 1,
		adc_i_clipped : 1,
		adc_aenc_clipped : 1,
		ENC_N : 1,
		ENC2_N : 1,
		AENC_N : 1,
		wd_err : 1;
	};
    uint32_t asInt;
    StatusFlags_s flags;
};


// Basic failsafe hardware boot config for inits
struct TMC4671MainConfig{
	TMC4671HardwareTypeConf hwconf;
	TMC4671MotConf motconf;
	uint16_t pwmcnt 		= 3999;
	uint8_t bbmL			= 10;
	uint8_t bbmH			= 10;
	uint16_t mdecA 			= 660; // 334 default. 331 recommended by datasheet,662 double. 660 lowest noise
	uint16_t mdecB 			= 660;
	uint32_t mclkA			= 0x20000000; //0x20000000 default
	uint32_t mclkB			= 0x20000000; // For AENC
	uint16_t adc_I0_offset 	= 33415;
	uint16_t adc_I1_offset 	= 33415;
	uint16_t adc_I0_scale	= 256;
	uint16_t adc_I1_scale	= 256;
	bool svpwm				= true; // enable space vector PWM for 3 phase motors
	bool canChangeHwType 	= true; // Allows changing the hardware version by commands
};

struct TMC4671PIDConf{
	uint16_t fluxI		= 512;
	uint16_t fluxP		= 256;
	uint16_t torqueI	= 512;
	uint16_t torqueP	= 256;
	uint16_t velocityI	= 0;
	uint16_t velocityP	= 256;
	uint16_t positionI	= 0;
	uint16_t positionP	= 128;
	bool sequentialPI	= false; // Advanced pid
};

struct TMC4671Limits{
	uint16_t pid_torque_flux_ddt	= 32767;
	uint16_t pid_uq_ud				= 30000;
	uint16_t pid_torque_flux		= 32767;
	uint32_t pid_acc_lim			= 2147483647;
	uint32_t pid_vel_lim			= 2147483647;
	int32_t pid_pos_low				= -2147483647;
	int32_t pid_pos_high			= 2147483647;
};

struct TMC4671PidPrecision{ // Switch between Q8.8 (false) and Q4.12 (true) precision for pid controller
	bool current_I	= false;
	bool current_P	= false;
	bool velocity_I	= false;
	bool velocity_P	= false;
	bool position_I	= false;
	bool position_P	= false;
};

struct TMC4671FlashAddrs{
	uint16_t mconf = ADR_TMC1_MOTCONF;
	uint16_t cpr = ADR_TMC1_CPR;
	uint16_t encA = ADR_TMC1_ENCA;
	uint16_t offsetFlux = ADR_TMC1_OFFSETFLUX;
	uint16_t torque_p = ADR_TMC1_TORQUE_P;
	uint16_t torque_i = ADR_TMC1_TORQUE_I;
	uint16_t flux_p = ADR_TMC1_FLUX_P;
	uint16_t flux_i = ADR_TMC1_FLUX_I;
	uint16_t ADC_i0_ofs = ADR_TMC1_ADC_I0_OFS;
	uint16_t ADC_i1_ofs = ADR_TMC1_ADC_I1_OFS;
	uint16_t encOffset = ADR_TMC1_ENC_OFFSET;
	uint16_t phieOffset = ADR_TMC1_PHIE_OFS;
};

struct TMC4671ABNConf{
	uint32_t cpr = 10000;
	bool apol 	= true;
	bool bpol 	= true;
	bool npol	= true;
	bool npos 	= false;
	bool rdir 	= false;
	bool ab_as_n = false;
	bool latch_on_N = false; // Restore ABN_DECODER_COUNT_N into encoder count if true on pulse. otherwise store encoder count in ABN_DECODER_COUNT_N
	int16_t phiEoffset = 0;	// Depends on phiM!
	int16_t phiMoffset = 0;
	int16_t posOffsetFromIndex = 0; // offset position to load after homing
	bool useIndex = false;

};

struct TMC4671AENCConf{
	uint32_t cpr = 1; //0x40
	int16_t phiAoffset = 0; // Main offset. 0x3e

	int16_t phiEoffset = 0; // 0x45&0xffff
	int16_t phiMoffset = 0;	// 0x45&0xffff0000

	uint16_t AENC0_offset = 0x7fff;
	int16_t AENC0_scale = 256;
	uint16_t AENC1_offset = 0x7fff;
	int16_t AENC1_scale = 256;
	uint16_t AENC2_offset = 20000;
	int16_t AENC2_scale = 256;

	int16_t nMask = 0; // 0x3c & 0xffff0000
	int16_t nThreshold = 0; // 0x3c & 0xffff
	// 0x3b
	bool uvwmode = false; // 0 = sincos, 1 = uvw
	bool rdir = false;

};

struct TMC4671HALLConf{
	bool hallEnabled = false;
	bool polarity = true;
	bool interpolation = true;
	bool direction = false;
	bool filter = true;
	uint16_t blank = 100;
	int16_t pos0 = 0;
	int16_t pos60 = 10922;
	int16_t pos120 = 21845;
	int16_t pos180 = -32768;
	int16_t pos240 = -21846;
	int16_t pos300 = -10923;
	int16_t phiEoffset = 0;
	int16_t phiMoffset = 0;
	uint16_t dPhiMax = 10922;
};

struct TMC4671Biquad{
	int32_t a1 = 0;
	int32_t a2 = 0;
	int32_t b0 = 0;
	int32_t b1 = 0;
	int32_t b2 = 0;
	bool enable = false;
};


class TMC4671 :
		public MotorDriver, public PersistentStorage, public Encoder,
		public CommandHandler, public SPIDevice, public ExtiHandler, public cpp_freertos::Thread,
#ifdef TIM_TMC
		public TimerHandler
#endif
{

	enum class TMC4671_commands : uint32_t{
		cpr,mtype,encsrc,tmcHwType,encalign,poles,acttrq,pwmlim,
		torqueP,torqueI,fluxP,fluxI,velocityP,velocityI,posP,posI,
		tmctype,pidPrec,phiesrc,fluxoffset,seqpi,tmcIscale,encdir,temp,reg,
		svpwm,fullCalibration,abnindexenabled,findIndex
	};

public:

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	const ClassType getClassType() override {return ClassType::Motordriver;};

	static TMC4671MotConf decodeMotFromInt(uint16_t val);
	static uint16_t encodeMotToInt(TMC4671MotConf mconf);


	TMC4671(SPIPort& spiport,OutputPin cspin = OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin),uint8_t address=1);

	void setHwType(TMC_HW_Ver type);

	void setAddress(uint8_t address);

	uint8_t getSpiAddr();
	bool setSpiAddr(uint8_t chan);
	virtual ~TMC4671();

	TMC4671MainConfig conf;

	bool initialize();
	void initializeWithPower();

	void Run();
	bool motorReady();

	bool hasPower();
	int32_t getTmcVM();
	bool isSetUp();

	uint32_t readReg(uint8_t reg);
	void writeReg(uint8_t reg,uint32_t dat);
	void updateReg(uint8_t reg,uint32_t dat,uint32_t mask,uint8_t shift);
	//void SpiTxCplt(SPI_HandleTypeDef *hspi);

	void setMotorType(MotorType motor,uint16_t poles);

	void runOpenLoop(uint16_t ud,uint16_t uq,int32_t speed,int32_t accel,bool torqueMode = false);
	void setOpenLoopSpeedAccel(int32_t speed,uint32_t accel);

	// Setup routines
	bool calibrateAdcOffset(uint16_t time = 500);
	void setup_ABN_Enc(TMC4671ABNConf encconf);
	void setup_AENC(TMC4671AENCConf encconf);
	void setup_HALL(TMC4671HALLConf hallconf);
	void bangInitEnc(int16_t power);
	void estimateABNparams();
	bool checkEncoder();
	void calibrateAenc();
	void calibrateEncoder();

	void setEncoderType(EncoderType_TMC type);
	uint32_t getEncCpr();

	void setAdcOffset(uint32_t adc_I0_offset,uint32_t adc_I1_offset);
	void setAdcScale(uint32_t adc_I0_scale,uint32_t adc_I1_scale);

	void setupFeedForwardTorque(int32_t gain, int32_t constant);
	void setupFeedForwardVelocity(int32_t gain, int32_t constant);
	void setFFMode(FFMode mode);
	void setSequentialPI(bool sequential);
	bool feedforward = false;

	void setBiquadFlux(TMC4671Biquad bq);
	void setBiquadTorque(TMC4671Biquad bq);
	void setBiquadPos(TMC4671Biquad bq);
	void setBiquadVel(TMC4671Biquad bq);
	

	bool pingDriver();
	std::pair<uint32_t,std::string> getTmcType();

	void changeState(TMC_ControlState newState);

#ifdef TIM_TMC
	void timerElapsed(TIM_HandleTypeDef* htim);
#endif

	void setPositionExt(int32_t pos); // External position register (For external encoders. Choose external phiE).

	void stopMotor();
	void startMotor();

	void emergencyStop();
	bool emergency = false;
	bool estopTriggered = false;
	void turn(int16_t power);
	int16_t nextFlux = 0;
	int16_t idleFlux = 0;
	uint16_t maxOffsetFlux = 0;

	int16_t bangInitPower = 5000; // Default current in setup routines

	void setTorque(int16_t torque);

	void setTargetPos(int32_t pos);
	int32_t getTargetPos();
	void setTargetVelocity(int32_t vel);
	int32_t getTargetVelocity();
	int32_t getVelocity();
	int16_t getTorque();
	void setFlux(int16_t flux);
	int16_t getFlux();
	void setFluxTorque(int16_t flux, int16_t torque);
	void setFluxTorqueFF(int16_t flux, int16_t torque);
	std::pair<int32_t,int32_t> getActualCurrent();

	float getTemp();
	TMC_ControlState getState();

	void setPhiEtype(PhiE type);
	PhiE getPhiEtype();
	void setPhiE_ext(int16_t phiE);
	int16_t getPhiE();
	void setPhiE(int16_t phiE);

	void setPosSel(PosSelection psel);
	void setVelSel(VelSelection vsel,uint8_t mode = 0);

	void setMotionMode(MotionMode mode,bool force = false); // force true sets it immediately. use false to change mode when tmc is ready on startMotor
	MotionMode getMotionMode();

	void setUdUq(int16_t ud,int16_t uq);
	void setBrakeLimits(uint16_t low, uint16_t high); // Raw brake resistor limits (see tmc reg 0x75)

	bool reachedPosition(uint16_t tolerance);

	// Pids
	void setPids(TMC4671PIDConf pids);
	TMC4671PIDConf getPids();
	TMC4671PIDConf curPids;

	TMC4671Limits curLimits;
	void setLimits(TMC4671Limits limits);
	TMC4671Limits getLimits();

	void setUqUdLimit(uint16_t limit);
	void setTorqueLimit(uint16_t limit);

	void setPidPrecision(TMC4671PidPrecision setting);
	TMC4671PidPrecision pidPrecision;

	TMC4671ABNConf abnconf;
	TMC4671HALLConf hallconf;
	TMC4671AENCConf aencconf;

	//Encoder
	Encoder* getEncoder() override;
	bool hasIntegratedEncoder() override;
	int32_t getPos() override;
	int32_t getPosAbs() override;
	void setPos(int32_t pos) override;
	void setTmcPos(int32_t pos);
	//uint32_t getPosCpr();
	uint32_t getCpr();
	void setCpr(uint32_t cpr);
	EncoderType getType() override;
	uint32_t posToEnc(uint32_t pos);
	uint32_t encToPos(uint32_t enc);

	void exti(uint16_t GPIO_Pin);
	void encoderIndexHit();
	bool findEncoderIndex(int32_t speed=10, uint16_t power=2500,bool offsetPhiM=false,bool zeroCount=false);
	bool autohome();
	void zeroAbnUsingPhiM();

	StatusFlags readFlags(bool maskedOnly = true);
	void setStatusMask(StatusFlags mask);
	void setStatusMask(uint32_t mask); // Mask for status pin.
	void setStatusFlags(uint32_t flags);
	void setStatusFlags(StatusFlags flags);
	void setEncoderIndexFlagEnabled(bool enabled,bool zeroEncoder = false);
	void statusCheck();
	bool flagCheckInProgress = false;
	StatusFlags statusFlags = {0};
	StatusFlags statusMask = {0};

	void saveFlash() override;
	void restoreFlash() override;
	TMC4671FlashAddrs flashAddrs;

	uint16_t encodeEncHallMisc();
	void restoreEncHallMisc(uint16_t val);

	bool allowSlowSPI = true; // For engineering sample

	void beginSpiTransfer(SPIPort* port);
	void endSpiTransfer(SPIPort* port);
	//void spiTxCompleted(SPIPort* port);

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands();

	virtual std::string getHelpstring(){return "TMC4671 interface";}


private:
	OutputPin enablePin = OutputPin(*DRV_ENABLE_GPIO_Port,DRV_ENABLE_Pin);

	const Error lowVoltageError = Error(ErrorCode::undervoltage,ErrorType::warning,"Low motor voltage");
	const Error communicationError = Error(ErrorCode::tmcCommunicationError, ErrorType::warning, "TMC not responding");

	ENC_InitState encstate = ENC_InitState::uninitialized;
	TMC_ControlState state = TMC_ControlState::uninitialized;
	TMC_ControlState laststate = TMC_ControlState::uninitialized;
	TMC_ControlState laststateNopower;
	MotionMode curMotionMode = MotionMode::stop;
	MotionMode lastMotionMode = MotionMode::stop;
	MotionMode nextMotionMode = MotionMode::stop;

	TMC_StartupType startupType = TMC_StartupType::NONE;

	bool ES_TMCdetected = false; // ES versions are not made anymore and have some critical issues

	bool encoderAligned = false;
	//bool encoderIndexFound = false;
	bool initialized = false; // Init ran once
	bool adcCalibrated = false;
	bool adcSettingsRestored = false;
	bool motorEnabledRequested = false;
	volatile bool encoderIndexHitFlag = false;
	bool zeroEncoderOnIndexHit = false;
	bool fullCalibrationInProgress = false;
	bool phiErestored = false;
	//int32_t phiEOffsetRestored = 0; //-0x8000 to 0x7fff



	uint8_t enc_retry = 0;
	uint8_t enc_retry_max = 3;

	uint32_t lastStatTime = 0;

	uint8_t spi_buf[5] = {0};

	void initAdc(uint16_t mdecA, uint16_t mdecB,uint32_t mclkA,uint32_t mclkB);
	void setPwm(uint8_t val,uint16_t maxcnt,uint8_t bbmL,uint8_t bbmH);// 100MHz/maxcnt+1
	void setPwm(TMC_PwmMode val);// pwm mode
	void setSvPwm(bool enable);
	void encInit();

	void saveAdcParams();


// state machine
	void ABN_init();
	void AENC_init();

	uint32_t initTime = 0;
	bool manualEncAlign = false;
	bool spiActive = false; // Flag for tx interrupt that the transfer was started by this instance

	// External encoder timer fires interrupts to trigger a new commutation position update
#ifdef TIM_TMC

	TIM_HandleTypeDef* externalEncoderTimer = &TIM_TMC;
#else
	TIM_HandleTypeDef* externalEncoderTimer = nullptr;
#endif
	void setUpExtEncTimer();
};


class TMC_1 : public TMC4671 {
public:
	TMC_1() : TMC4671{motor_spi,OutputPin(*SPI1_SS1_GPIO_Port, SPI1_SS1_Pin),1} {}

	//const ClassIdentifier getInfo() override;
	static bool isCreatable();
	static ClassIdentifier info;
};

class TMC_2 : public TMC4671 {
public:
	TMC_2() : TMC4671{motor_spi,OutputPin(*SPI1_SS2_GPIO_Port, SPI1_SS2_Pin),2} {}

	//const ClassIdentifier getInfo() override;
	static bool isCreatable();
	static ClassIdentifier info;
};

#endif /* TMC4671_H_ */
