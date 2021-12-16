#ifndef __CONSTANTS_H
#define __CONSTANTS_H

//#include "main.h"
#include "target_constants.h" // To be defined in target include folder

/*
 * For more settings see target_constants.h in a target specific folder
 */


//#define SW_VERSION "1.5.0" // Version string
static const uint8_t SW_VERSION_INT[3] = {1,5,1}; // Version as array. 8 bit each!
//#define MIN_SW_CONFIGURATOR "1.5.0" // Minimal supported configurator version. to be removed in a later version!

#define MAX_AXIS 2 // ONLY USE 2 for now else screws HID Reports


#ifndef CANBUS
#undef ODRIVE
#undef VESC
#endif

#endif
