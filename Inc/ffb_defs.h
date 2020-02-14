/*
 * ffb_defs.h
 *
 *  Created on: Feb 9, 2020
 *      Author: Yannick
 */

#ifndef FFB_DEFS_H_
#define FFB_DEFS_H_

#include "cppmain.h"
#include "Filters.h"

#define FFB_ID_OFFSET 0x00
#define MAX_EFFECTS 40

// HID Descriptor definitions - Axes
#define HID_USAGE_X		0x30
#define HID_USAGE_Y		0x31
#define HID_USAGE_Z		0x32
#define HID_USAGE_RX	0x33
#define HID_USAGE_RY	0x34
#define HID_USAGE_RZ	0x35
#define HID_USAGE_SL0	0x36
#define HID_USAGE_SL1	0x37
#define HID_USAGE_WHL	0x38
#define HID_USAGE_POV	0x39

// HID Descriptor definitions - FFB Effects
#define HID_USAGE_CONST 0x26    //    Usage ET Constant Force
#define HID_USAGE_RAMP  0x27    //    Usage ET Ramp
#define HID_USAGE_SQUR  0x30    //    Usage ET Square
#define HID_USAGE_SINE  0x31    //    Usage ET Sine
#define HID_USAGE_TRNG  0x32    //    Usage ET Triangle
#define HID_USAGE_STUP  0x33    //    Usage ET Sawtooth Up
#define HID_USAGE_STDN  0x34    //    Usage ET Sawtooth Down
#define HID_USAGE_SPRNG 0x40    //    Usage ET Spring
#define HID_USAGE_DMPR  0x41    //    Usage ET Damper
#define HID_USAGE_INRT  0x42    //    Usage ET Inertia
#define HID_USAGE_FRIC  0x43    //    Usage ET Friction


// HID Descriptor definitions - FFB Report IDs
#define HID_ID_STATE	0x02	// Usage PID State report

#define HID_ID_EFFREP	0x01	// Usage Set Effect Report
#define HID_ID_ENVREP	0x02	// Usage Set Envelope Report
#define HID_ID_CONDREP	0x03	// Usage Set Condition Report
#define HID_ID_PRIDREP	0x04	// Usage Set Periodic Report
#define HID_ID_CONSTREP	0x05	// Usage Set Constant Force Report
#define HID_ID_RAMPREP	0x06	// Usage Set Ramp Force Report
#define HID_ID_CSTMREP	0x07	// Usage Custom Force Data Report
#define HID_ID_SMPLREP	0x08	// Usage Download Force Sample
#define HID_ID_EFOPREP	0x0A	// Usage Effect Operation Report
#define HID_ID_BLKFRREP	0x0B	// Usage PID Block Free Report
#define HID_ID_CTRLREP	0x0C	// Usage PID Device Control
#define HID_ID_GAINREP	0x0D	// Usage Device Gain Report
#define HID_ID_SETCREP	0x0E	// Usage Set Custom Force Report
// Features
#define HID_ID_NEWEFREP	0x11	// Usage Create New Effect Report
#define HID_ID_BLKLDREP	0x12	// Usage Block Load Report
#define HID_ID_POOLREP	0x13	// Usage PID Pool Report

#define FFB_EFFECT_NONE		0x00
#define FFB_EFFECT_CONSTANT		0x01
#define FFB_EFFECT_RAMP			0x02
#define FFB_EFFECT_SQUARE 		0x03
#define FFB_EFFECT_SINE 		0x04
#define FFB_EFFECT_TRIANGLE		0x05
#define FFB_EFFECT_SAWTOOTHUP	0x06
#define FFB_EFFECT_SAWTOOTHDOWN	0x07
#define FFB_EFFECT_SPRING		0x08
#define FFB_EFFECT_DAMPER		0x09
#define FFB_EFFECT_INERTIA		0x0A
#define FFB_EFFECT_FRICTION		0x0B
#define FFB_EFFECT_CUSTOM	0x0C

#define HID_ACTUATOR_POWER 0x08
#define HID_SAFETY_SWITCH 0x04
#define HID_ENABLE_ACTUATORS 0x02
#define HID_ENABLE_ACTUATORS_MASK 0xFD
#define HID_EFFECT_PLAYING 0x10

// Only include these for cpp
#ifdef __cplusplus
struct  __attribute__((__packed__)) reportHID_t {
		uint8_t id = 1;
		int16_t buttons = 0;
		int16_t X = 0;
		int16_t Y = 0;
		int16_t Z = 0;
		int16_t RX = 0;
		int16_t RY = 0;
		int16_t RZ = 0;
		int16_t Slider = 0;
	};

typedef struct
{
	const uint8_t	reportId = HID_ID_STATE+FFB_ID_OFFSET;
	uint8_t	effectBlockIndex = 1;	//EffectId (1..40)
	uint8_t	status = (HID_ACTUATOR_POWER) | (HID_ENABLE_ACTUATORS);	// Bits: 0=Device Paused,1=Actuators Enabled,2=Safety Switch,3=Actuator Power, 4=Effect Playing

} __attribute__((packed)) reportFFB_status_t;



typedef struct
	{ // FFB: Set Effect Output Report
	uint8_t		reportId = 1;	// =1
	uint8_t		effectBlockIndex = 0;	// 1..max_effects
	uint8_t		effectType = 0;	// 1..12 (effect usages: 26,27,30,31,32,33,34,40,41,42,43,28)
	uint16_t	duration = 0; // 0..32767 ms
	uint16_t	triggerRepeatInterval = 0; // 0..32767 ms
	uint16_t	samplePeriod = 0;	// 0..32767 ms
	uint8_t		gain = 255;	// 0..255	 (physical 0..10000)
	uint8_t		triggerButton = 0;	// button ID (0..8)
	uint8_t		enableAxis = 0; // bits: 0=X, 1=Y, 2=DirectionEnable
	uint8_t		directionX = 0;	// angle (0=0 .. 255=360deg)
	uint8_t		directionY = 0;	// angle (0=0 .. 255=360deg)
//	uint16_t	typeSpecificBlockOffsetX = 0; // Needed?
//	uint16_t	typeSpecificBlockOffsetY = 0;
//	uint16_t	startDelay;	// 0..32767 ms
} __attribute__((packed)) FFB_SetEffect_t;

typedef struct
	{ // FFB: Set Condition Output Report
	uint8_t	reportId;	//
	uint8_t	effectBlockIndex;	// 1..40
	uint8_t	parameterBlockOffset;	// bits: 0..3=parameterBlockOffset, 4..5=instance1, 6..7=instance2
	int16_t  cpOffset;	// -128..127
	int16_t	positiveCoefficient;
	int16_t	negativeCoefficient;
	uint16_t	positiveSaturation;
	uint16_t	negativeSaturation;
	uint16_t	deadBand;
} __attribute__((packed)) FFB_SetCondition_Data_t;



typedef struct
	{ // FFB: PID Block Load Feature Report
	uint8_t	reportId = HID_ID_BLKLDREP;	// =2
	uint8_t effectBlockIndex;	// 1..40
	uint8_t	loadStatus;	// 1=Success,2=Full,3=Error
	uint16_t	ramPoolAvailable;	// =0 or 0xFFFF?
} __attribute__((packed)) FFB_BlockLoad_Feature_Data_t;

typedef struct
	{ // FFB: Create New Effect Feature Report
	uint8_t		reportId;
	uint8_t	effectType;	// Enum (1..12): ET 26,27,30,31,32,33,34,40,41,42,43,28
	uint16_t	byteCount;	// 0..511
} __attribute__((packed)) FFB_CreateNewEffect_Feature_Data_t;

typedef struct
	{ // FFB: PID Pool Feature Report
	uint8_t	reportId = HID_ID_POOLREP;
	uint16_t	ramPoolSize = MAX_EFFECTS;
	uint8_t		maxSimultaneousEffects = MAX_EFFECTS;
	uint8_t		memoryManagement = 3;	// Bits: 0=DeviceManagedPool, 1=SharedParameterBlocks
} __attribute__((packed)) FFB_PIDPool_Feature_Data_t;


typedef struct
	{ // FFB: Set Periodic Output Report
	uint8_t	reportId;	// =4
	uint8_t	effectBlockIndex;
	uint16_t magnitude;
	int16_t	offset;
	uint16_t	phase;	// degrees
	uint16_t	period;	// 0..32767 ms
} __attribute__((packed)) FFB_SetPeriodic_Data_t;

typedef struct
{
	volatile uint8_t state = 0;
	uint8_t type=FFB_EFFECT_NONE;
	uint8_t gain=255;
	int16_t	positiveCoefficient=0;
	int16_t	negativeCoefficient=0;
	uint16_t	positiveSaturation=0;
	uint16_t	negativeSaturation=0;
	int16_t magnitude = 0;
	int16_t phase=0;
	int16_t offset=0;
	int32_t last_value = 0;
	Biquad* filter = nullptr;
	uint16_t counter=0;						// ms
	uint16_t period=0;							// ms
	uint16_t duration=0,fadeTime=0,attackTime=0;	// ms
	uint16_t samplePeriod = 0;
	uint8_t axis = 0;
	uint16_t	deadBand = 0;
} FFB_Effect;



// --------------- Effects------------------------
typedef struct
	{ // FFB: Set ConstantForce Output Report
	uint8_t	reportId;	// =5
	uint8_t	effectBlockIndex;	// 1..40
	int16_t magnitude;	// -10000..10000 (set by hid descriptor report)
} __attribute__((packed)) FFB_SetConstantForce_Data_t;

#endif //c++

#endif /* FFB_DEFS_H_ */
