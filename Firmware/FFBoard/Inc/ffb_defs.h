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
#include "constants.h" // For #define MAX_AXIS
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

// Control
#define HID_ID_CUSTOMCMD 0xAF   // Custom cmd (old. reserved)
#define HID_ID_HIDCMD	 0xA1   // HID cmd
#define HID_ID_STRCMD	 0xAC   // HID cmd as string. reserved
//#define HID_ID_CUSTOMCMD_IN 0xA2   // Custom cmd in. reserved
//#define HID_ID_CUSTOMCMD_OUT 0xA1   // Custom cmd out. reserved


#define FFB_EFFECT_NONE			0x00
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
#define FFB_EFFECT_CUSTOM		0x0C

#define HID_ACTUATOR_POWER 		0x08
#define HID_SAFETY_SWITCH 		0x04
#define HID_ENABLE_ACTUATORS 	0x02
#define HID_EFFECT_PAUSE		0x01
#define HID_ENABLE_ACTUATORS_MASK 0xFD
#define HID_EFFECT_PLAYING 		0x10

#define HID_DIRECTION_ENABLE 0x04
#define FFB_EFFECT_DURATION_INFINITE 0xffff

// Only include these for cpp
#ifdef __cplusplus

// HID gamepad report


struct  __attribute__((__packed__)) reportHID_t {
		uint8_t id = 1;
		uint64_t buttons = 0;
		int16_t X = 0;
		int16_t Y = 0;
		int16_t Z = 0;
		int16_t RX = 0;
		int16_t RY = 0;
		int16_t RZ = 0;
		int16_t Dial = 0;
		int16_t Slider = 0;
};

/*
 * Helper function to access analog axes in packed HID report struct
 */
inline void setHidReportAxis(reportHID_t *report, uint8_t idx, int16_t val){
	switch(idx){
	case 0:
		report->X = val;
		break;
	case 1:
		report->Y = val;
		break;
	case 2:
		report->Z = val;
		break;
	case 3:
		report->RX = val;
		break;
	case 4:
		report->RY = val;
		break;
	case 5:
		report->RZ = val;
		break;
	case 6:
		report->Dial = val;
		break;
	case 7:
		report->Slider = val;
		break;
	default:
		return;
	}
}

typedef struct
{
	const uint8_t	reportId = HID_ID_STATE+FFB_ID_OFFSET;
	//uint8_t	effectBlockIndex = 1;	//EffectId
	uint8_t	status = (HID_ACTUATOR_POWER) | (HID_ENABLE_ACTUATORS);	// Bits: 0=Device Paused,1=Actuators Enabled,2=Safety Switch,3=Actuator Power, 4=Effect Playing

} __attribute__((packed)) reportFFB_status_t;



typedef struct
	{
	uint8_t		reportId = 1;
	uint8_t		effectBlockIndex = 0;	// 1..max_effects
	uint8_t		effectType = 0;
	uint16_t	duration = 0; // 0..32767 ms
	uint16_t	triggerRepeatInterval = 0; // 0..32767 ms
	uint16_t	samplePeriod = 0;	// 0..32767 ms
	uint16_t	startDelay = 0;	// 0..32767 ms
	uint8_t		gain = 255;	// 0..255 scaler
	uint8_t		triggerButton = 0;	// button ID. unused
	uint8_t		enableAxis = 0; // bits: 0=X, 1=Y, 2=DirectionEnable
	uint16_t	directionX = 0;	// angle (0=0 .. 36000=360deg)
	uint16_t	directionY = 0;	// angle (0=0 .. 36000=360deg) TODO axes are last bytes in struct if fewer axes are used. use different report if this is not enough anymore!
//#if MAX_AXIS == 3
//	uint8_t directionZ = 0; // angle (0=0 .. 255=360deg)
//#endif
	//	uint16_t	typeSpecificBlockOffsetX = 0; // Needed?
	//	uint16_t	typeSpecificBlockOffsetY = 0;
//	uint16_t	startDelay;	// 0..32767 ms
} __attribute__((packed)) FFB_SetEffect_t;

typedef struct
	{
	uint8_t		reportId;
	uint8_t		effectBlockIndex;	// 1..max_effects
	uint8_t		parameterBlockOffset;	// bits: 0..3=parameterBlockOffset, 4..5=instance1, 6..7=instance2
	int16_t  	cpOffset;	// Center
	int16_t		positiveCoefficient; // Scaler for positive range
	int16_t		negativeCoefficient;
	uint16_t	positiveSaturation;	// Clipping point for positive range
	uint16_t	negativeSaturation;
	uint16_t	deadBand;
} __attribute__((packed)) FFB_SetCondition_Data_t;


typedef struct
	{
	//uint8_t		reportId; // ID removed by tinyusb
	uint8_t	effectType;	// Effect type ID
	uint16_t	byteCount;	// Size of custom effects
} __attribute__((packed)) FFB_CreateNewEffect_Feature_Data_t;

// Feature GET report
typedef struct
	{
	//uint8_t	reportId = HID_ID_BLKLDREP; // No report ID for tinyusb feature GET
	uint8_t effectBlockIndex;	// 1..max_effects
	uint8_t	loadStatus;	// 1=Success,2=Full,3=Error
	uint16_t	ramPoolAvailable;
} __attribute__((packed)) FFB_BlockLoad_Feature_Data_t;

typedef struct
	{
	//uint8_t	reportId = HID_ID_POOLREP; // No report ID for tinyusb feature GET
	uint16_t	ramPoolSize = MAX_EFFECTS;
	uint8_t		maxSimultaneousEffects = MAX_EFFECTS;
	uint8_t		memoryManagement = 1;	// 0=DeviceManagedPool (0/1), 1=SharedParameterBlocks (0/1)
} __attribute__((packed)) FFB_PIDPool_Feature_Data_t;


typedef struct
	{
	uint8_t	reportId;
	uint8_t	effectBlockIndex;
	uint16_t magnitude;
	int16_t	offset;
	uint16_t	phase;	// degrees
	uint32_t	period;	// 0..32767 ms
} __attribute__((packed)) FFB_SetPeriodic_Data_t;

typedef struct
{
	uint8_t reportId;
	uint8_t effectBlockIndex;
	uint16_t attackLevel;
	uint16_t fadeLevel;
	uint32_t attackTime;
	uint32_t fadeTime;
} __attribute__((packed)) FFB_SetEnvelope_Data_t;

typedef struct
{
	uint8_t reportId;
	uint8_t effectBlockIndex;
	uint16_t startLevel;
	uint16_t endLevel;
} __attribute__((packed)) FFB_SetRamp_Data_t;

typedef struct
{
	int16_t cpOffset = 0; // Center point
	int16_t positiveCoefficient = 0;
	int16_t negativeCoefficient = 0;
	uint16_t positiveSaturation = 0;
	uint16_t negativeSaturation = 0;
	uint16_t deadBand = 0;

	bool isActive(){ // Condition is active if either coefficient is not zero
		return (positiveCoefficient != 0 && positiveSaturation != 0) || (negativeCoefficient != 0 && negativeSaturation != 0);
	}
} FFB_Effect_Condition;

typedef struct
{
	uint8_t reportId;
	uint8_t effectBlockIndex;
	uint8_t state;
	uint8_t loopCount;
} __attribute__((packed)) FFB_EffOp_Data_t;

// Internal struct for storing effects
typedef struct
{
	volatile uint8_t state = 0;
	uint8_t type = FFB_EFFECT_NONE; // Type
	int16_t offset = 0;				// Center point
	uint8_t gain = 255;				// Scaler. often unused
	int16_t magnitude = 0;			// High res intensity of effect
	int16_t startLevel = 0;			// Ramp effect
	int16_t endLevel = 0;			// Ramp effect
	float axisMagnitudes[MAX_AXIS] = {0}; // 0=0,100%=1

	FFB_Effect_Condition conditions[MAX_AXIS];
	int16_t phase = 0;
	uint16_t period = 0;
	uint32_t duration = FFB_EFFECT_DURATION_INFINITE;					 // Duration in ms
	uint16_t attackLevel = 0, fadeLevel = 0; // Envelope effect
	uint32_t attackTime = 0, fadeTime = 0;	 // Envelope effect

	std::unique_ptr<Biquad> filter[MAX_AXIS] = { nullptr };  // Optional filter
	uint16_t startDelay = 0;
	uint32_t startTime = 0;	  // Elapsed time in ms before effect starts
	uint16_t samplePeriod = 0;
	bool useEnvelope = false;
	bool useSingleCondition = true;
} FFB_Effect;



// --------------- Effects------------------------
typedef struct
	{
	uint8_t	reportId;
	uint8_t	effectBlockIndex;	// 1..max_effects
	int16_t magnitude;	// High res intensity
} __attribute__((packed)) FFB_SetConstantForce_Data_t;

#endif //c++

#endif /* FFB_DEFS_H_ */
