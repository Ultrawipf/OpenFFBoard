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
#include "usb_device.h"


void cppmain();
void usb_init();

#ifdef __cplusplus
}

//extern std::unique_ptr<FFBoardMain> mainclass;

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

void refreshWatchdog(); // Refreshes the watchdog

#endif

#endif /* CPPMAIN_H_ */
