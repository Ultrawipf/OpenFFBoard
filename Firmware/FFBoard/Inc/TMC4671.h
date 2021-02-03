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

#define SPITIMEOUT 100
extern SPI_HandleTypeDef HSPIDRV;

enum class TMC_ControlState {uninitialized,No_power,Shutdown,Running,Init_wait,ABN_init,AENC_init,Enc_bang,HardError,OverTemp};
enum class ENC_InitState {uninitialized,estimating,aligning,checking,OK};

enum class MotorType : uint8_t {NONE=0,DC=1,STEPPER=2,BLDC=3,ERR};
enum class PhiE : uint8_t {ext=1,openloop=2,abn=3,hall=5,aenc=6,aencE=7,NONE};
enum class MotionMode : uint8_t {stop=0,torque=1,velocity=2,position=3,prbsflux=4,prbstorque=5,prbsvelocity=6,uqudext=8,encminimove=9,NONE};
enum class FFMode : uint8_t {none=0,velocity=1,torque=2};
enum class PosSelection : uint8_t {PhiE=0, PhiE_ext=1, PhiE_openloop=2, PhiE_abn=3, res1=4, PhiE_hal=5, PhiE_aenc=6, PhiA_aenc=7, res2=8, PhiM_abn=9, PhiM_abn2=10, PhiM_aenc=11, PhiM_hal=12};
enum class EncoderType_TMC : uint8_t {NONE=0,abn=1,sincos=2,uvw=3,hall=4,ext=5}; // max 7

struct TMC4671MotConf{
	MotorType motor_type = MotorType::NONE; //saved
	EncoderType_TMC enctype = EncoderType_TMC::NONE; //saved
	PhiE phiEsource 	= PhiE::ext;
	uint16_t pole_pairs = 4; //saved
	PosSelection pos_sel = PosSelection::PhiE;
};


//#define TMCTEMP

const int thermistor_R2 = 1500, tmcOffset = 1000; //400? Offset is how many ADC counts too high does the tmc read due to current flowing from the ADC. Can be calculated by measuring the pin voltage with adc on and off.

// Beta-Model
const float thermistor_Beta = 4300, thermistor_R = 22000;
const float temp_limit = 100; //°C

// Basic failsafe hardware boot config for inits
struct TMC4671MainConfig{

	TMC4671MotConf motconf;
	uint16_t pwmcnt = 3999;
	uint8_t bbmL	= 10;
	uint8_t bbmH	= 10;
	uint16_t mdecA 	= 668; // 334 default. 668 for lower noise
	uint16_t mdecB 	= 668;
	uint32_t mclkA	= 0x20000000; //0x20000000 default
	uint32_t mclkB	= 0x20000000; // For AENC
	uint32_t adc_I0_offset 	= 33415;
	uint32_t adc_I1_offset 	= 33415;
	uint32_t adc_I0_scale	= 256;
	uint32_t adc_I1_scale	= 256;
};

struct TMC4671PIDConf{
	uint16_t fluxI		= 512;
	uint16_t fluxP		= 128;
	uint16_t torqueI	= 512;
	uint16_t torqueP	= 128;
	uint16_t velocityI	= 0;
	uint16_t velocityP	= 128;
	uint16_t positionI	= 0;
	uint16_t positionP	= 64;
	bool sequentialPI	= true;
};

struct TMC4671Limits{
	uint16_t pid_torque_flux_ddt	= 32767;
	uint16_t pid_uq_ud				= 23169;
	uint16_t pid_torque_flux		= 32767;
	uint32_t pid_acc_lim			= 2147483647;
	uint32_t pid_vel_lim			= 2147483647;
	int32_t pid_pos_low				= -2147483647;
	int32_t pid_pos_high			= 2147483647;
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
};

struct TMC4671ABNConf{
	uint32_t cpr = 8192;
	bool apol 	= true;
	bool bpol 	= true;
	bool npol	= true;
	bool npos 	= false;
	bool rdir 	= false;
	bool ab_as_n = false;
	bool latch_on_N = false; // Latch offsets on n pulse
	int16_t phiEoffset = 0;
	int16_t phiMoffset = 0;
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

	int16_t nMask = 0; // 0x3c0xffff0000
	int16_t nThreshold = 0; // 0x3c&0xffff
	// 0x3b
	bool uvwmode = false; // 0 = sincos, 1 = uvw
	bool rdir = false;

};

struct TMC4671HALLConf{
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


class TMC4671 : public MotorDriver, public Encoder, public CommandHandler, public SpiHandler{
public:

	static ClassIdentifier info;
	const ClassIdentifier getInfo();

	static TMC4671MotConf decodeMotFromInt(uint16_t val);
	static uint16_t encodeMotToInt(TMC4671MotConf mconf);

	//SPI_HandleTypeDef* spi = &HSPIDRV,GPIO_TypeDef* csport=SPI1_SS1_GPIO_Port,uint16_t cspin=SPI1_SS1_Pin
	TMC4671(SPI_HandleTypeDef* spi,GPIO_TypeDef* csport,uint16_t cspin,TMC4671MainConfig conf);
	TMC4671();

	void setAddress(uint8_t addr);
	uint8_t getAddress();

	virtual ~TMC4671();

	TMC4671MainConfig conf;

	bool initialize();
	bool initialized = false;
	void update();

	bool hasPower();
	bool isSetUp();

	uint32_t readReg(uint8_t reg);
	void writeReg(uint8_t reg,uint32_t dat);
	void updateReg(uint8_t reg,uint32_t dat,uint32_t mask,uint8_t shift);
	void SpiTxCplt(SPI_HandleTypeDef *hspi);

	void setMotorType(MotorType motor,uint16_t poles);

	void runOpenLoop(uint16_t ud,uint16_t uq,int32_t speed,int32_t accel);
	void setOpenLoopSpeedAccel(int32_t speed,uint32_t accel);

	// Setup routines
	void calibrateAdcOffset();
	void setup_ABN_Enc(TMC4671ABNConf encconf);
	void setup_AENC(TMC4671AENCConf encconf);
	void setup_HALL(TMC4671HALLConf hallconf);
	void bangInitEnc(int16_t power);
	void estimateABNparams();
	bool checkEncoder();
	void calibrateAenc();

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

	void setPositionExt(int32_t pos); // External position register (For external encoders. Choose external phiE).
	// Contact me if external support has to be added via the FFB selection!

	void stop();
	void start();
	bool active = false;
	void emergencyStop();
	bool emergency = false;
	bool estopTriggered = false;
	void turn(int16_t power);
	int16_t nextFlux = 0;
	int16_t idleFlux = 0;
	uint16_t maxOffsetFlux = 0;

	bool useSvPwm = true;

	int16_t bangInitPower = 5000;
	bool encoderInitialized = false;

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
	int32_t getActualCurrent();

	float getTemp();

	void setPhiEtype(PhiE type);
	PhiE getPhiEtype();
	void setPhiE_ext(int16_t phiE);

	void setPosSel(PosSelection psel);

	void setMotionMode(MotionMode mode);
	MotionMode getMotionMode();

	void setUdUq(int16_t ud,int16_t uq);
	void setBrakeLimits(uint16_t low, uint16_t high); // Raw brake resistor limits (see tmc reg 0x75)

	bool reachedPosition(uint16_t tolerance);
	void setStatusMask(uint32_t mask); // Mask for status pin. If multiple are parallel disable this for now

	// Pids
	void setPids(TMC4671PIDConf pids);
	TMC4671PIDConf getPids();
	TMC4671PIDConf curPids;

	TMC4671Limits curLimits;
	void setLimits(TMC4671Limits limits);
	TMC4671Limits getLimits();

	TMC4671ABNConf abnconf;
	TMC4671HALLConf hallconf;
	TMC4671AENCConf aencconf;

	//Encoder
	int32_t getPos();
	void setPos(int32_t pos);
	//uint32_t getPosCpr();
	uint32_t getCpr();
	void setCpr(uint32_t cpr);
	EncoderType getType();
	uint32_t posToEnc(uint32_t pos);
	uint32_t encToPos(uint32_t enc);


	void saveFlash();
	void restoreFlash();
	TMC4671FlashAddrs flashAddrs;

	uint16_t encodeEncHallMisc();
	void restoreEncHallMisc(uint16_t val);

	bool allowSlowSPI = true; // For engineering sample

	ParseStatus command(ParsedCommand* cmd,std::string* reply);
	virtual std::string getHelpstring(){
		return "TMC4671 commands:\n"
				"mtype,encsrc,encalign,poles,phiesrc,reg,fluxoffset\n"
				"torqueP,torqueI,fluxP,fluxI\n"
				"acttrq,seqpi,tmctemp\n";}


private:
	ENC_InitState encstate = ENC_InitState::uninitialized;
	TMC_ControlState state = TMC_ControlState::uninitialized;
	MotionMode curMotionMode = MotionMode::stop;
	bool oldTMCdetected = false;

	uint8_t enc_retry = 0;
	uint8_t enc_retry_max = 5;

	uint32_t lastStatTime = 0;

	uint8_t address = 1;

	SPI_HandleTypeDef* spi = &HSPIDRV;
	GPIO_TypeDef* csport=SPI1_SS1_GPIO_Port;
	uint16_t cspin=SPI1_SS1_Pin;
	uint8_t spi_buf[5];

	void initAdc(uint16_t mdecA, uint16_t mdecB,uint32_t mclkA,uint32_t mclkB);
	void setPwm(uint8_t val,uint16_t maxcnt,uint8_t bbmL,uint8_t bbmH);// 100MHz/maxcnt+1
	void setSvPwm(bool enable);
	void encInit();


// state machine
	void ABN_init();
	void AENC_init();

	uint32_t initTime = 0;

	volatile bool spi_busy = false;

};

#endif /* TMC4671_H_ */
