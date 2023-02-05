/*
 * cppmain.h
 *
 *  Created on: Jan 4, 2020
 *      Author: Yannick
 */

#ifndef CPPMAIN_H_
#define CPPMAIN_H_

#include "constants.h"

#ifndef DEFAULTMAIN
#define DEFAULTMAIN 0
#endif

#pragma once
#ifdef __cplusplus

#include "FFBoardMain.h"
#include "ledEffects.h"
#include "ClassChooser.h"



extern "C" {
#endif

#include "eeprom_addresses.h"
#include "main.h"
#include "cmsis_compiler.h"


void cppmain();
void usb_init();
void tudThread(void *argument);

#ifdef __cplusplus
}

static inline bool inIsr(){
	return (__get_PRIMASK() != 0U) || (__get_IPSR() != 0U);
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

uint32_t micros(); // Returns microsecond scaled time
unsigned long getRunTimeCounterValue(void); // RTOS

void refreshWatchdog(); // Refreshes the watchdog

#endif


#endif /* CPPMAIN_H_ */
