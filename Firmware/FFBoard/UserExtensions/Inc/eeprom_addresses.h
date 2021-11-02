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
#define NB_OF_VAR	76

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
#define ADR_FFBWHEEL_BUTTONCONF 		0x101
#define ADR_FFBWHEEL_ANALOGCONF 		0x102
#define ADR_FFBWHEEL_CONF1		 		0x103


// Button Sources:
#define ADR_SPI_BTN_1_CONF				0x201
#define ADR_SHIFTERANALOG_CONF			0x202
#define ADR_LOCAL_BTN_CONF				0x203
#define ADR_LOCAL_BTN_CONF_2			0x204
#define ADR_SPI_BTN_2_CONF				0x205
#define ADR_SPI_BTN_1_CONF_2            0x206
#define ADR_SPI_BTN_2_CONF_2            0x207

#define ADR_ENCLOCAL_CPR				0x210

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


#define ADR_CF_FILTER       			0x280 // CF Lowpass

// How many axis configured 1-3
#define ADR_AXIS_COUNT					0x300
#define ADR_AXIS_EFFECTS1			    0x310 // 0-7 inertia, 8-15 friction
#define ADR_AXIS_EFFECTS2			    0x350 // 0-7 spring, 8-15 damper
//#define ADR_AXIS_EFFECTS3	    		0x390 // unused

// AXIS1
#define ADR_AXIS1_CONFIG				0x301 // 0-2 ENC, 3-5 DRV
#define ADR_AXIS1_POWER			    	0x302
#define ADR_AXIS1_DEGREES		    	0x303
#define ADR_AXIS1_MAX_SPEED				0x304 // Store the max speed
#define ADR_AXIS1_MAX_ACCEL				0x305 // Store the max accel
#define ADR_AXIS1_ENDSTOP		    	0x307 // 0-7 endstop margin, 8-15 endstop stiffness
#define ADR_AXIS1_EFFECTS1		    	0x308 // 0-7 idlespring, 8-15 damper


// TMC1
#define ADR_TMC1_MOTCONF 				0x320 // 0-2: MotType 3-5: PhiE source 6-15: Poles
#define ADR_TMC1_CPR					0x321
#define ADR_TMC1_ENCA					0x322

#define ADR_TMC1_OFFSETFLUX				0x326
#define ADR_TMC1_TORQUE_P				0x327
#define ADR_TMC1_TORQUE_I				0x328
#define ADR_TMC1_FLUX_P					0x329
#define ADR_TMC1_FLUX_I					0x32A


// AXIS2
#define ADR_AXIS2_CONFIG				0x341 // 0-2 ENC, 3-5 DRV
#define ADR_AXIS2_POWER	    			0x342
#define ADR_AXIS2_DEGREES	    		0x343
#define ADR_AXIS2_MAX_SPEED				0x344 // Store the max speed
#define ADR_AXIS2_MAX_ACCEL				0x345 // Store the max accel
#define ADR_AXIS2_ENDSTOP		    	0x347 // 0-7 endstop margin, 8-15 endstop stiffness
#define ADR_AXIS2_EFFECTS1		    	0x348 // 0-7 idlespring, 8-15 damper


// TMC2
#define ADR_TMC2_MOTCONF 				0x360 // 0-2: MotType 3-5: PhiE source 6-15: Poles
#define ADR_TMC2_CPR					0x361
#define ADR_TMC2_ENCA					0x362

#define ADR_TMC2_OFFSETFLUX				0x366
#define ADR_TMC2_TORQUE_P				0x367
#define ADR_TMC2_TORQUE_I				0x368
#define ADR_TMC2_FLUX_P					0x369
#define ADR_TMC2_FLUX_I					0x36A


// AXIS3
#define ADR_AXIS3_CONFIG				0x381 // 0-2 ENC, 3-5 DRV
#define ADR_AXIS3_POWER			    	0x382
#define ADR_AXIS3_DEGREES			    0x383
#define ADR_AXIS3_MAX_SPEED				0x384 // Store the max speed
#define ADR_AXIS3_MAX_ACCEL				0x385 // Store the max accel
#define ADR_AXIS3_ENDSTOP	    		0x387 // 0-7 endstop margin, 8-15 endstop stiffness
#define ADR_AXIS3_EFFECTS1		    	0x388 // 0-7 idlespring, 8-15 damper


// TMC3
#define ADR_TMC3_MOTCONF 				0x3A0 // 0-2: MotType 3-5: PhiE source 6-15: Poles
#define ADR_TMC3_CPR					0x3A1
#define ADR_TMC3_ENCA					0x3A2

#define ADR_TMC3_OFFSETFLUX				0x3A6
#define ADR_TMC3_TORQUE_P				0x3A7
#define ADR_TMC3_TORQUE_I				0x3A8
#define ADR_TMC3_FLUX_P					0x3A9
#define ADR_TMC3_FLUX_I					0x3AA


// Odrive
#define ADR_ODRIVE_CANID				0x3D0 //0-6 ID M0, 7-12 ID M1, 13-15 can speed
#define ADR_ODRIVE_SETTING1_M0			0x3D1
#define ADR_ODRIVE_SETTING1_M1			0x3D2


// Vesc
#define ADR_VESC_CANDATA				0x3E0 //0-8 AxisCanID , 9-11 can speed, 12 useEncoder
#define ADR_VESC_OFFSET					0x3E1

//MT Encoder
#define ADR_MTENC_CONF1					0x3F0

#endif /* EEPROM_ADDRESSES_H_ */
