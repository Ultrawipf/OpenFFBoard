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

#define SPITIMEOUT 100
extern SPI_HandleTypeDef HSPIDRV;

enum class MotorType : uint8_t {NONE=0,DC=1,STEPPER=2,BLDC=3,ERR};
enum class PhiE : uint8_t {ext=1,openloop=2,abn=3,hall=5,aenc=6,aencE=7,NONE};
enum class MotionMode : uint8_t {stop=0,torque=1,velocity=2,position=3,prbsflux=4,prbstorque=5,prbsvelocity=6,uqudext=8,encminimove=9,NONE};
enum class FFMode : uint8_t {none=0,velocity=1,torque=2};

struct TMC4671MotConf{
	MotorType motor_type = MotorType::NONE;
	PhiE phiEsource 	= PhiE::ext;
	uint16_t pole_pairs = 50;
};

// Basic failsafe hardware boot config for inits
struct TMC4671MainConfig{

	TMC4671MotConf motconf;
	uint16_t pwmcnt = 3999;
	uint8_t bbmL	= 5;
	uint8_t bbmH	= 5;
	uint16_t mdecA 	= 668; // 334 default
	uint16_t mdecB 	= 0;
	uint32_t mclkA	= 0x20000000; //0x20000000 default
	uint32_t mclkB	= 0;
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
	uint16_t ppr = ADR_TMC1_PPR;
	uint16_t encA = ADR_TMC1_ENCA;
	uint16_t offsetFlux = ADR_TMC1_OFFSETFLUX;
	uint16_t torque_p = ADR_TMC1_TORQUE_P;
	uint16_t torque_i = ADR_TMC1_TORQUE_I;
	uint16_t flux_p = ADR_TMC1_FLUX_P;
	uint16_t flux_i = ADR_TMC1_FLUX_I;
};

struct TMC4671ABNConf{
	uint32_t ppr = 8192;
	bool apol 	= true;
	bool bpol 	= true;
	bool npol	= true;
	bool npos 	= false;
	bool rdir 	= false;
	bool ab_as_n = false;
	bool latch_on_N = false; // Latch offsets on n pulse
	int16_t phiEoffset = 0;
	int16_t phiMoffset = 0;
	int16_t bangInitPower = 4000;
	bool initialized = false;
};

struct TMC4671HALLConf{
	bool polarity = false;
	bool interpolation = true;
	bool direction = false;
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


class TMC4671 : public MotorDriver, public Encoder, public PersistentStorage, public CommandHandler{
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

	void setMotorType(MotorType motor,uint16_t poles);

	void runOpenLoop(uint16_t ud,uint16_t uq,int32_t speed,int32_t accel);
	void setOpenLoopSpeedAccel(int32_t speed,uint32_t accel);

	// Setup routines
	void calibrateAdcOffset();
	void calibrateAdcScale();
	void setup_ABN_Enc(TMC4671ABNConf encconf);
	void setup_HALL(TMC4671HALLConf hallconf);
	void bangInitABN(int16_t power);
	void alignABN();
	bool findABNPol();
	bool checkABN();
	bool findABNDir();

	void setAdcOffset(uint32_t adc_I0_offset,uint32_t adc_I1_offset);
	void setAdcScale(uint32_t adc_I0_scale,uint32_t adc_I1_scale);

	void setupFeedForwardTorque(int32_t gain, int32_t constant);
	void setupFeedForwardVelocity(int32_t gain, int32_t constant);
	void setFFMode(FFMode mode);
	bool feedforward = false;

	void setBiquadFlux(TMC4671Biquad bq);
	void setBiquadTorque(TMC4671Biquad bq);
	void setBiquadPos(TMC4671Biquad bq);
	void setBiquadVel(TMC4671Biquad bq);

	void stop();
	void start();
	bool active = false;
	void emergencyStop();
	bool emergency = false;
	void turn(int16_t power);
	int16_t nextFlux = 0;
	int16_t idleFlux = 0;
	uint16_t maxOffsetFlux = 0;

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

	void setPhiEtype(PhiE type);
	PhiE getPhiEtype();
	void setPhiE_ext(int16_t phiE);

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

	bool command(ParsedCommand* cmd,std::string* reply);

private:
	MotionMode curMotionMode = MotionMode::stop;
	bool oldTMCdetected = false;

	uint8_t address = 1;

	SPI_HandleTypeDef* spi = &HSPIDRV;
	GPIO_TypeDef* csport=SPI1_SS1_GPIO_Port;
	uint16_t cspin=SPI1_SS1_Pin;

	void initAdc(uint16_t mdecA, uint16_t mdecB,uint32_t mclkA,uint32_t mclkB);
	void setPwm(uint8_t val,uint16_t maxcnt,uint8_t bbmL,uint8_t bbmH);// 100MHz/maxcnt+1
	void encInit();

	uint32_t initTime = 0;

};

#endif /* TMC4671_H_ */
