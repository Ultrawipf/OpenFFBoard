#ifndef __CONSTANTS_H
#define __CONSTANTS_H

//#include "main.h"
#include "target_constants.h" // To be defined in target include folder

/*
 * For more settings see target_constants.h in a target specific folder
 */




static const uint8_t SW_VERSION_INT[3] = {1,8,0}; // Version as array. 8 bit each!

#define MAX_AXIS 2 // ONLY USE 2 for now else screws HID Reports


#ifndef CANBUS
#undef ODRIVE
#undef CANBUTTONS
#undef VESC
#endif


#ifdef FFBWHEEL
#define AXIS1_FFB_HID_DESC
#endif

#ifdef FFBJOYSTICK
#define AXIS2_FFB_HID_DESC
#endif

#endif
