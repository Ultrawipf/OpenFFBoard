#ifndef __CONSTANTS_H
#define __CONSTANTS_H

//#include "main.h"
#include "target_constants.h" // To be defined in target include folder

/*
 * For more settings see target_constants.h in a target specific folder
 */

static const uint8_t SW_VERSION_INT[3] = {1,16,4}; // Version as array. 8 bit each!
#ifndef MAX_AXIS
#define MAX_AXIS 2 // ONLY USE 2 for now else screws HID Reports
#endif
#if !(MAX_AXIS > 0 && MAX_AXIS <= 3)
#error "MAX_AXIS must be between 1 and 3"
#endif

#define FLASH_VERSION 0 // Counter to increase whenever a full flash erase is required.

//#define DEBUGLOG // Uncomment to enable some debug printouts

#ifndef CANBUS
#undef ODRIVE
#undef CANBUTTONS
#undef VESC
#undef CANBRIDGE
#undef CANINPUTMAIN
#endif


#ifdef FFBWHEEL
#ifdef FFBWHEEL_USE_1AXIS_DESC
#define AXIS1_FFB_HID_DESC
#else
#define AXIS2_FFB_HID_DESC
#endif
#endif

#ifdef FFBJOYSTICK
#define AXIS2_FFB_HID_DESC
#endif

#ifndef HSPIDRV
#undef TMC4671DRIVER
#endif

#ifdef FFBHIDEXT
#define FFB_HID_DESC_GAMEPAD
#endif

#ifndef ENCODER_SPI_PORT
#define ENCODER_SPI_PORT ext3_spi // See cpp_target_config.cpp for ports
#endif

#ifdef SIMPLEMOTION
#define GPIO_MOTOR // See cpp_target_config.cpp for pin
#endif

#if defined(I2C_PORT_EEPROM) && defined(I2C_EEPROM_ADR)
#define USE_I2C_EEPROM
#else
#if defined(EEPROM_START_ADDRESS)
#define USE_EEPROM_EMULATION
#endif
#endif

#if defined(PCF8574BUTTONS) || defined(I2C_PORT)
#define I2CBUS
#endif


#ifndef TEMPSENSOR_ADC_RES
#define TEMPSENSOR_ADC_RES 0
#endif

// Assume vref is calibrated in full resolution. Check chip and __LL_ADC_CALC_VREFANALOG_VOLTAGE how to scale
#ifndef VREF_ADC_RES
#define VREF_ADC_RES 0
#endif

#ifndef VSENSE_ADC_RES
#define VSENSE_ADC_RES 0
#endif

#ifndef ADC_INTREF_VOL
	#ifndef ADC_INTREF_VAL
		#define ADC_INTREF_VOL 3300
	#else
		#define ADC_INTREF_VOL ((ADC_INTREF_VAL > 0) ? __LL_ADC_CALC_VREFANALOG_VOLTAGE(ADC_INTREF_VAL,VREF_ADC_RES) : 0)
	#endif
#endif

#ifndef VOLTAGE_MULT_DEFAULT
#define VOLTAGE_MULT_DEFAULT 30.67 // mV adc * scaler = voltage for 976k/33k divider and 3.3V vref
#endif

#if defined(TIM_MICROS_HALTICK) && defined(TIM_MICROS)
#error "Only TIM_MICROS_HALTICK OR TIM_MICROS may be defined as a microsecond timebase"
#endif


#endif
