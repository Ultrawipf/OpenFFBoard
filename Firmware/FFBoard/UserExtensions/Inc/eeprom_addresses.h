/*		
 * eeprom_addresses.h		
 *		
 *  Created on: 24.01.2020		
 *  Author: Yannick		
 *		
 *	/!\ Generated from the file memory_map.csv 
   / ! \ DO NOT EDIT THIS DIRECTLY !!!	
 */			
		
#ifndef EEPROM_ADDRESSES_H_
#define EEPROM_ADDRESSES_H_

#include "main.h"
// Change this to the amount of currently registered variables
#define NB_OF_VAR 164
extern const uint16_t VirtAddVarTab[NB_OF_VAR];

// Amount of variables in exportable list
#define NB_EXPORTABLE_ADR 149
extern const uint16_t exportableFlashAddresses[NB_EXPORTABLE_ADR];


		
/* Add your addresses here. 0xffff is invalid as it marks an erased field.		
Anything below 0x00ff is reserved for system variables.		
		
Use ranges that are clear to distinguish between configurations. Address ranges can have gaps.		
Label the names clearly.		
Example: 0x0100 - 0x01ff for one class and 0x0200-0x02ff for another class would be reasonable even if they each need only 3 variables		
		
		
Important: Add your variable to the VirtAddVarTab[NB_OF_VAR] array in eeprom_addresses.c!		
		
Tip to check if a cell is intialized:		
uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data) will return 1 if the address is not found or 0 if it was found.		
*/
// System variables
#define ADR_HW_VERSION 1
#define ADR_SW_VERSION 2
#define ADR_FLASH_VERSION 4
#define ADR_CURRENT_CONFIG 10
// Ports
#define ADR_CANCONF1 0xC1
#define ADR_I2CCONF1 0xC2
// FFBWheel
#define ADR_FFBWHEEL_BUTTONCONF 0x101
#define ADR_FFBWHEEL_ANALOGCONF 0x102
#define ADR_FFBWHEEL_CONF1 0x103
// CAN remote
#define ADR_CANREMOTE_CONF1 0x120
#define ADR_CANREMOTE_CONF2 0x121
// Button Sources:
#define ADR_SPI_BTN_1_CONF 0x201
#define ADR_SHIFTERANALOG_CONF 0x202
#define ADR_LOCAL_BTN_CONF 0x203 // Pin mask
#define ADR_LOCAL_BTN_CONF_2 0x204 // Misc settings
#define ADR_SPI_BTN_2_CONF 0x205
#define ADR_SPI_BTN_1_CONF_2 0x206
#define ADR_SPI_BTN_2_CONF_2 0x207
#define ADR_LOCAL_BTN_CONF_3 0x208 // Pulse mask
// Local encoder
#define ADR_ENCLOCAL_CPR 0x210
#define ADR_ENCLOCAL_OFS 0x211
// PWM
#define ADR_PWM_MODE 0x220
// Local analog source
#define ADR_LOCALANALOG_MASK 0x230
// Shifter Analog
#define ADR_SHIFTERANALOG_X_12 0x240
#define ADR_SHIFTERANALOG_X_56 0x241
#define ADR_SHIFTERANALOG_Y_135 0x242
#define ADR_SHIFTERANALOG_Y_246 0x243
#define ADR_SHIFTERANALOG_CONF_2 0x244
#define ADR_SHIFTERANALOG_CONF_3 0x245
// PCF buttons
#define ADR_PCFBTN_CONF1 0x250
// CAN port
#define ADR_CANBTN_CONF1 0x260
#define ADR_CANBTN_CONF2 0x261 // CAN ID
// CAN analog
#define ADR_CANANALOG_CONF1 0x270
// FFB Engine flash area
#define ADR_FFB_CF_FILTER 0x280 // Constant Force Lowpass
#define ADR_FFB_FR_FILTER 0x281 // Friction Lowpass
#define ADR_FFB_DA_FILTER 0x282 // Damper Lowpass
#define ADR_FFB_IN_FILTER 0x283 // Inertia Lowpass
#define ADR_FFB_EFFECTS1 0x284 // 0-7 inertia, 8-15 friction
#define ADR_FFB_EFFECTS2 0x285 // 0-7 spring, 8-15 damper
#define ADR_FFB_EFFECTS3 0x286 // 0-7 friction ramp up zone, 8-9 filterProfile
// Button Sources:
#define ADR_ADS111X_CONF1 0x290
// How many axis configured 1-3
#define ADR_AXIS_COUNT 0x300
// AXIS1
#define ADR_AXIS1_CONFIG 0x301 // 0-2 ENC, 3-5 DRV
#define ADR_AXIS1_POWER 0x302
#define ADR_AXIS1_DEGREES 0x303
#define ADR_AXIS1_MAX_SPEED 0x304 // Store the max speed
#define ADR_AXIS1_MAX_ACCEL 0x305 // Store the max accel
#define ADR_AXIS1_ENDSTOP 0x307 // 0-7 endstop margin, 8-15 endstop stiffness
#define ADR_AXIS1_EFFECTS1 0x308 // 0-7 idlespring, 8-15 damper
#define ADR_AXIS1_SPEEDACCEL_FILTER 0x309 // Speed/Accel filter Lowpass profile
#define ADR_AXIS1_ENC_RATIO 0x30A // Accel filter Lowpass
#define ADR_AXIS1_EFFECTS2 0x30B // 0-7 Friction, 8-15 Inertia
#define ADR_AXIS1_POSTPROCESS1 0x30C // 0-7 expo curve
// TMC1
#define ADR_TMC1_MOTCONF 0x320 // 0-2: MotType 3-5: PhiE source 6-15: Poles
#define ADR_TMC1_CPR 0x321
#define ADR_TMC1_ENCA 0x322 // Misc
#define ADR_TMC1_ADC_I0_OFS 0x323
#define ADR_TMC1_ADC_I1_OFS 0x324
#define ADR_TMC1_ENC_OFFSET 0x325
#define ADR_TMC1_OFFSETFLUX 0x326
#define ADR_TMC1_TORQUE_P 0x327
#define ADR_TMC1_TORQUE_I 0x328
#define ADR_TMC1_FLUX_P 0x329
#define ADR_TMC1_FLUX_I 0x32A
#define ADR_TMC1_PHIE_OFS 0x32B
#define ADR_TMC1_TRQ_FILT 0x32C
// AXIS2
#define ADR_AXIS2_CONFIG 0x341 // 0-2 ENC, 3-5 DRV
#define ADR_AXIS2_POWER 0x342
#define ADR_AXIS2_DEGREES 0x343
#define ADR_AXIS2_MAX_SPEED 0x344 // Store the max speed
#define ADR_AXIS2_MAX_ACCEL 0x345 // Store the max accel
#define ADR_AXIS2_ENDSTOP 0x347 // 0-7 endstop margin, 8-15 endstop stiffness
#define ADR_AXIS2_EFFECTS1 0x348 // 0-7 idlespring, 8-15 damper
#define ADR_AXIS2_SPEEDACCEL_FILTER 0x349 // Speed/Accel filter Lowpass profile
#define ADR_AXIS2_ENC_RATIO 0x34A // Store the encoder ratio for an axis
#define ADR_AXIS2_EFFECTS2 0x34B // 0-7 Friction, 8-15 Inertia
#define ADR_AXIS2_POSTPROCESS1 0x34C // 0-7 expo curve
// TMC2
#define ADR_TMC2_MOTCONF 0x360 // 0-2: MotType 3-5: PhiE source 6-15: Poles
#define ADR_TMC2_CPR 0x361
#define ADR_TMC2_ENCA 0x362 // Misc
#define ADR_TMC2_ADC_I0_OFS 0x363
#define ADR_TMC2_ADC_I1_OFS 0x364
#define ADR_TMC2_ENC_OFFSET 0x365
#define ADR_TMC2_OFFSETFLUX 0x366
#define ADR_TMC2_TORQUE_P 0x367
#define ADR_TMC2_TORQUE_I 0x368
#define ADR_TMC2_FLUX_P 0x369
#define ADR_TMC2_FLUX_I 0x36A
#define ADR_TMC2_PHIE_OFS 0x36B
#define ADR_TMC2_TRQ_FILT 0x36C
// AXIS3
#define ADR_AXIS3_CONFIG 0x381 // 0-2 ENC, 3-5 DRV
#define ADR_AXIS3_POWER 0x382
#define ADR_AXIS3_DEGREES 0x383
#define ADR_AXIS3_MAX_SPEED 0x384 // Store the max speed
#define ADR_AXIS3_MAX_ACCEL 0x385 // Store the max accel
#define ADR_AXIS3_ENDSTOP 0x387 // 0-7 endstop margin, 8-15 endstop stiffness
#define ADR_AXIS3_EFFECTS1 0x388 // 0-7 idlespring, 8-15 damper
#define ADR_AXIS3_SPEEDACCEL_FILTER 0x389 // Speed/Accel filter Lowpass profile
#define ADR_AXIS3_ENC_RATIO 0x38A // Store the encoder ratio for an axis
#define ADR_AXIS3_EFFECTS2 0x38B // 0-7 Friction, 8-15 Inertia
#define ADR_AXIS3_POSTPROCESS1 0x38C // 0-7 expo curve
// TMC3
#define ADR_TMC3_MOTCONF 0x3A0 // 0-2: MotType 3-5: PhiE source 6-15: Poles
#define ADR_TMC3_CPR 0x3A1
#define ADR_TMC3_ENCA 0x3A2 // Misc
#define ADR_TMC3_ADC_I0_OFS 0x3A3
#define ADR_TMC3_ADC_I1_OFS 0x3A4
#define ADR_TMC3_ENC_OFFSET 0x3A5
#define ADR_TMC3_OFFSETFLUX 0x3A6
#define ADR_TMC3_TORQUE_P 0x3A7
#define ADR_TMC3_TORQUE_I 0x3A8
#define ADR_TMC3_FLUX_P 0x3A9
#define ADR_TMC3_FLUX_I 0x3AA
#define ADR_TMC3_PHIE_OFS 0x3AB
#define ADR_TMC3_TRQ_FILT 0x3AC
// RMD CAN Motor
#define ADR_RMD1_DATA1 0x3C0 //0-4 CAN ID
#define ADR_RMD1_TORQUE 0x3C1 //Maximum current
#define ADR_RMD1_OFFSET 0x3C2 //Position offset
#define ADR_RMD2_DATA1 0x3C3
#define ADR_RMD2_TORQUE 0x3C4
#define ADR_RMD2_OFFSET 0x3C5
// Odrive
#define ADR_ODRIVE_CANID 0x3D0 //0-6 ID M0, 7-12 ID M1, 13-15 can speed
#define ADR_ODRIVE_SETTING1_M0 0x3D1
#define ADR_ODRIVE_SETTING1_M1 0x3D2
#define ADR_ODRIVE_OFS_M0 0x3D3 // Encoder offset position reload
#define ADR_ODRIVE_OFS_M1 0x3D4
// VESC Section
#define ADR_VESC1_CANID 0x3E0 //0-7 AxisCanID, 8-16 VescCanId
#define ADR_VESC1_DATA 0x3E1 //0-2 can speed, 3 useVescEncoder
#define ADR_VESC1_OFFSET 0x3E2 //16b offset
#define ADR_VESC2_CANID 0x3E3 //0-8 AxisCanID, 8-16 VescCanId
#define ADR_VESC2_DATA 0x3E4 //0-2 can speed, 3 useVescEncoder
#define ADR_VESC2_OFFSET 0x3E5 //16b offset
#define ADR_VESC3_CANID 0x3E6 //0-8 AxisCanID, 8-16 VescCanId
#define ADR_VESC3_DATA 0x3E7 //0-2 can speed, 3 useVescEncoder
#define ADR_VESC3_OFFSET 0x3E8 //16b offset
//MT Encoder
#define ADR_MTENC_OFS 0x400
#define ADR_MTENC_CONF1 0x401
// Biss-C
#define ADR_BISSENC_CONF1 0x410
#define ADR_BISSENC_OFS 0x411
// SSI
#define ADR_SSI_CONF1 0x413
#define ADR_SSI_OFS 0x414
// Analog min/max calibrations
#define ADR_LOCALANALOG_MIN_0 0x500
#define ADR_LOCALANALOG_MAX_0 0x501
#define ADR_LOCALANALOG_MIN_1 0x502
#define ADR_LOCALANALOG_MAX_1 0x503
#define ADR_LOCALANALOG_MIN_2 0x504
#define ADR_LOCALANALOG_MAX_2 0x505
#define ADR_LOCALANALOG_MIN_3 0x506
#define ADR_LOCALANALOG_MAX_3 0x507
#define ADR_LOCALANALOG_MIN_4 0x508
#define ADR_LOCALANALOG_MAX_4 0x509
#define ADR_LOCALANALOG_MIN_5 0x50A
#define ADR_LOCALANALOG_MAX_5 0x50B
#define ADR_LOCALANALOG_MIN_6 0x50C
#define ADR_LOCALANALOG_MAX_6 0x50D
#define ADR_LOCALANALOG_MIN_7 0x50E
#define ADR_LOCALANALOG_MAX_7 0x50F
// ADS111X
#define ADR_ADS111X_MIN_0 0x510
#define ADR_ADS111X_MAX_0 0x511
#define ADR_ADS111X_MIN_1 0x512
#define ADR_ADS111X_MAX_1 0x513
#define ADR_ADS111X_MIN_2 0x514
#define ADR_ADS111X_MAX_2 0x515
#define ADR_ADS111X_MIN_3 0x516
#define ADR_ADS111X_MAX_3 0x517
#endif /* EEPROM_ADDRESSES_H_ */
