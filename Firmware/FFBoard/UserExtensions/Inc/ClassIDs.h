/*
 * ClassIDs.h
 *
 *  Created on: 23.11.2021
 *      Author: Yannick
 */

#ifndef USEREXTENSIONS_INC_CLASSIDS_H_
#define USEREXTENSIONS_INC_CLASSIDS_H_
#include <stdint.h>
// Type IDs
enum class ClassType : uint16_t {
	NONE=0,
	Mainclass=1,
	Internal=2,
	Motordriver=3,
	Buttonsource=4,
	Analogsource=5,
	Encoder=6,
	Axis=7,
	Extension=8,
	Port=9
};

// Mainclasses all have the name "main" in the commandhandler system for addressing and ID 0 but can have different display names
// Other classes will use their unique ID in the command system
#define CMDCLSID_MAIN		0x01
#define CMDCLSTR_MAIN		"main"

// The main command system can control the board independently.
#define CMDCLSID_SYS		0x00
#define CMDCLSTR_SYS		"sys"



// Unique class IDs. 16 bits (0xFFFF max)
#define CLSID_NONE			0	// Undefined IDs or failsafe main
#define CLSID_MAIN_FFBWHEEL 0x1
#define CLSID_MAIN_FFBJOY	0x2
#define CLSID_MAIN_FFBEXT	0x3
#define CLSID_MAIN_CANINPUT	0x5
#define CLSID_MAIN_TMCDBG 	0xB
#define CLSID_MAIN_CAN	 	0xC
#define CLSID_MAIN_MIDI 	0xD
#define CLSID_SYSTEM		0x10 // sys main command thread
#define CLSID_ERRORS		0x11

// Button sources for gamepad
#define CLSID_BTN_NONE		0x20
#define CLSID_BTN_LOCAL 	0x21
#define CLSID_BTN_SPI	 	0x22
#define CLSID_BTN_SHIFTER 	0x23
#define CLSID_BTN_PCF	 	0x24
#define CLSID_BTN_CAN	 	0x25


// Analog sources
#define CLSID_ANALOG_NONE 	0x40
#define CLSID_ANALOG_LOCAL 	0x41
#define CLSID_ANALOG_CAN	0x42
#define CLSID_ANALOG_ADS111X	0x43

// Encoders
#define CLSID_ENCODER_NONE	0x60
#define CLSID_ENCODER_LOCAL	0x61
#define CLSID_ENCODER_MTSPI	0x62
#define CLSID_ENCODER_BISS	0x63
#define CLSID_ENCODER_SSI	0x64

// Motordrivers
#define CLSID_MOT_NONE		0x80
#define CLSID_MOT_TMC0		0x81
#define CLSID_MOT_TMC1		0x82
#define CLSID_MOT_TMC2		0x83
#define CLSID_MOT_PWM		0x84
#define CLSID_MOT_ODRV0		0x85
#define CLSID_MOT_ODRV1		0x86
#define CLSID_MOT_VESC0		0x87
#define CLSID_MOT_VESC1		0x88
#define CLSID_MOT_SM1		0x89
#define CLSID_MOT_SM2		0x8A
#define CLSID_MOT_RMD1		0x8B
#define CLSID_MOT_RMD2		0x8C

// Internal classes
#define CLSID_AXIS			0xA01
#define CLSID_EFFECTSCALC	0xA02
#define CLSID_EFFECTSMGR	0xA03

#define CLSID_CANPORT		0xC01
#define CLSID_I2CPORT		0xC02

#define CLSID_CUSTOM		0x539 // Reserved for testing

#endif /* USEREXTENSIONS_INC_CLASSIDS_H_ */
