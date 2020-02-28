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
#define NB_OF_VAR	11

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

// FFBWheel 100-200 // TODO ADD
#define ADR_FFBWHEEL_CONFIG				0x101 // 0-2 ENC, 3-5 DRV
#define ADR_FFBWHEEL_POWER				0x102
#define ADR_FFBWHEEL_BUTTONCONF 		0x105
#define ADR_FFBWHEEL_ANALOGCONF 		0x106 // lower 8 bit mask, 8-9 offsetmode
#define ADR_TMC1_POLES_MOTTYPE_PHIE 	0x110 // 0-2: MotType 3-5: PhiE source 6-15: Poles
#define ADR_TMC1_PPR					0x111
#define ADR_TMC1_MISC					0x112

// Button Sources:
#define ADR_SPI_BTN_CONF				0x201
#define ADR_SHIFTER_BTN_CONF			0x202

#endif /* EEPROM_ADDRESSES_H_ */
