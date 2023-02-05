/*
 * cppmain.h
 *
 *  Created on: Jan 4, 2020
 *      Author: Yannick
 */

#ifndef CPPMAIN_H_
#define CPPMAIN_H_

#include "constants.h"

#pragma once
#ifdef __cplusplus

#include "FFBoardMain.h"
#include "ledEffects.h"
#include "ClassChooser.h"

extern "C" {
#endif

#include "eeprom_addresses.h"
#include "main.h"
#include "DebugLog.h"

void cppmain();
#ifndef HW_ESP32SX
#include "cmsis_compiler.h"
void usb_init();
void tudThread(void *argument);
#endif
#ifdef __cplusplus
}

static inline bool inIsr(){
#ifdef HW_ESP32SX
	return xPortInIsrContext();
#else
	return (__get_PRIMASK() != 0U) || (__get_IPSR() != 0U);
#endif
}

template<class T,class C>
int8_t cliptest(T v, C l, C h)
{
	if(v > h){
		return 1;
	}else if(v < l){
		return -1;
	}else{
		return 0;
	}
}

template<class T,class C>
T clip(T v, C l, C h)
{
  return { v > h ? h : v < l ? l : v };
}
#ifdef HW_ESP32SX
#define micros() esp_timer_get_time() // Returns microsecond scaled time
#else
uint32_t micros(); // Returns microsecond scaled time
unsigned long getRunTimeCounterValue(void); // RTOS

void refreshWatchdog(); // Refreshes the watchdog
#endif
#endif


#endif /* CPPMAIN_H_ */
