/*
 * flash_addresses.h
 *
 *  Created on: Jan 4, 2020
 *      Author: Yannick
 */

#ifndef EEPROM_ADDRESSES_H_
#define EEPROM_ADDRESSES_H_

#include "main.h"
// Change this to the amount of currently registered variables
#define NB_OF_VAR	34

extern uint16_t VirtAddVarTab[NB_OF_VAR];

/* Add your addresses here. 0xffff is invalid as it marks an erased field.
Anything below 0x00ff is reserved for system variables.

Use ranges that are clear to distinguish between configurations. Address ranges can have gaps.
Label the names clearly.
Example: 0x0100 - 0x01ff for one class and 0x0200-0x02ff for another class would be reasonable even if they each need only 3 variables


Important: Add your variable to the VirtAddVarTab[NB_OF_VAR] array in eeprom_addresses.c!

Tip to check if a cell is intialized:
uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data) will return 1 if the address is not found or 0 if it was found.
*/

// System variables:
#define ADR_HW_VERSION 		1
#define ADR_SW_VERSION 		2
#define ADR_CURRENT_CONFIG 	10

// FFBWheel
#define ADR_FFBWHEEL_CONFIG				0x101 // 0-2 ENC, 3-5 DRV
#define ADR_FFBWHEEL_POWER				0x102
#define ADR_FFBWHEEL_DEGREES			0x103

#define ADR_FFBWHEEL_BUTTONCONF 		0x105
#define ADR_FFBWHEEL_ANALOGCONF 		0x106
#define ADR_FFBWHEEL_ENDSTOP			0x107 // 0-7 endstop margin, 8-15 endstop stiffness


// TMC
#define ADR_TMC1_MOTCONF 				0x110 // 0-2: MotType 3-5: PhiE source 6-15: Poles
#define ADR_TMC1_CPR					0x111
#define ADR_TMC1_ENCA					0x112

#define ADR_TMC1_OFFSETFLUX				0x116
#define ADR_TMC1_TORQUE_P				0x117
#define ADR_TMC1_TORQUE_I				0x118
#define ADR_TMC1_FLUX_P					0x119
#define ADR_TMC1_FLUX_I					0x11A


// Button Sources:
#define ADR_SPI_BTN_1_CONF				0x201
#define ADR_SHIFTERANALOG_CONF			0x202
#define ADR_LOCAL_BTN_CONF				0x203
#define ADR_LOCAL_BTN_CONF_2			0x204
#define ADR_SPI_BTN_2_CONF				0x205
#define ADR_SPI_BTN_1_CONF_2            0x206
#define ADR_SPI_BTN_2_CONF_2            0x207

// FFB
#define ADR_FFB_EFFECTS1				0x210 // 0-7 spring, 8-15 friction
#define ADR_FFB_EFFECTS2				0x211 // CF Lowpass

// PWM
#define ADR_PWM_MODE					0x220

// Local analog source
#define ADR_LOCALANALOG_MASK			0x230

// Shifter Analog
#define ADR_SHIFTERANALOG_X_12			0x240
#define ADR_SHIFTERANALOG_X_56			0x241
#define ADR_SHIFTERANALOG_Y_135			0x242
#define ADR_SHIFTERANALOG_Y_246			0x243
#define ADR_SHIFTERANALOG_CONF_2		0x244
#define ADR_SHIFTERANALOG_CONF_3		0x245

#endif /* EEPROM_ADDRESSES_H_ */
