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
	int16_t bangInitPower = 8000;
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


class TMC4671 : public MotorDriver, public Encoder{
public:
	static TMC4671MotConf decodeMotFromInt(uint16_t val);
	static uint16_t encodeMotToInt(TMC4671MotConf mconf);

	//SPI_HandleTypeDef* spi = &HSPIDRV,GPIO_TypeDef* csport=SPI1_SS1_GPIO_Port,uint16_t cspin=SPI1_SS1_Pin
	TMC4671(SPI_HandleTypeDef* spi,GPIO_TypeDef* csport,uint16_t cspin,TMC4671MainConfig conf);
	virtual ~TMC4671();
	TMC4671MainConfig conf;

	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	bool initialize();
	bool initialized = false;
	void update();

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
	bool findABNPol();

	void setAdcOffset(uint32_t adc_I0_offset,uint32_t adc_I1_offset);
	void setAdcScale(uint32_t adc_I0_scale,uint32_t adc_I1_scale);

	void setupFeedForwardTorque(int32_t gain, int32_t constant);
	void setupFeedForwardVelocity(int32_t gain, int32_t constant);
	void setFFMode(FFMode mode);
	bool feedforward = false;

	void stop();
	void start();
	bool active = false;
	void emergencyStop();
	bool emergency = false;
	void turn(int16_t power);
	int16_t nextFlux = 0;
	int16_t idleFlux = 0;
	int16_t maxOffsetFlux = 3000;

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

	void setPhiEtype(PhiE type);
	PhiE getPhiEtype();
	void setPhiE_ext(int16_t phiE);

	void setMotionMode(MotionMode mode);
	MotionMode getMotionMode();

	void setUdUq(uint16_t ud,uint16_t uq);

	bool reachedPosition(uint16_t tolerance);
	void setStatusMask(uint32_t mask); // Mask for status pin. If multiple are parallel disable this for now



	// Pids
	void setPids(TMC4671PIDConf pids);
	TMC4671PIDConf getPids();
	TMC4671PIDConf curPids;

	TMC4671ABNConf abnconf;
	TMC4671HALLConf hallconf;
	int16_t idleFluxOffset = 500;

	//Encoder
	int32_t getPos();
	void setPos(int32_t pos);
	uint32_t getPosCpr();
	uint32_t getPpr();
	void setPpr(uint32_t cpr);
	uint32_t posToEnc(uint32_t pos);
	uint32_t encToPos(uint32_t enc);

private:
	MotionMode curMotionMode = MotionMode::stop;
	bool oldTMCdetected = false;
	SPI_HandleTypeDef* spi;
	uint16_t cspin;
	GPIO_TypeDef* csport;

	void initAdc(uint16_t mdecA, uint16_t mdecB,uint32_t mclkA,uint32_t mclkB);
	void setPwm(uint8_t val,uint16_t maxcnt,uint8_t bbmL,uint8_t bbmH);// 100MHz/maxcnt+1
	void encInit();

	uint32_t initTime = 0;

};

#endif /* TMC4671_H_ */
