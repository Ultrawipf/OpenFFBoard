#ifndef __CONSTANTS_H
#define __CONSTANTS_H

//#include "main.h"
#include "target_constants.h" // To be defined in target include folder

/*
 * For more settings see target_constants.h in a target specific folder
 */

static const uint8_t SW_VERSION_INT[3] = {1,13,1}; // Version as array. 8 bit each!
#define MAX_AXIS 2 // ONLY USE 2 for now else screws HID Reports
#define FLASH_VERSION 0 // Counter to increase whenever a full flash erase is required.

//#define DEBUGLOG // Uncomment to enable some debug printouts

#ifndef CANBUS
#undef ODRIVE
#undef CANBUTTONS
#undef VESC
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


#endif
