/*
 * global_callbacks.h
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#ifndef GLOBAL_CALLBACKS_H_
#define GLOBAL_CALLBACKS_H_

#include "main.h"

#pragma once
#ifdef __cplusplus

#include <vector>

// Helper to add a class to a handler
template <class C> void addCallbackHandler(std::vector<C>* vec, C instance){
	for(uint8_t i = 0; i < vec->size(); i++){
		if( (*vec)[i] == instance)
			return;
	}
	vec->push_back(instance);
}

template <class C> void removeCallbackHandler(std::vector<C>* vec, C instance){
	for (uint8_t i = 0; i < vec->size(); i++){
		if( (*vec)[i] == instance){
			vec->erase(vec->begin()+i);
			break;
		}
	}
}

extern "C" {
#endif

void startADC();
volatile uint32_t* getAnalogBuffer(ADC_HandleTypeDef* hadc,uint8_t* chans); // Returns the DMA buffer for a hadc reference
void CDC_Callback(uint8_t* Buf, uint32_t *Len);
void CDC_Finished();
void USBD_OutEvent_HID(uint8_t* report);
void USB_SOF();
void USBD_GetEvent_HID(uint8_t id,uint16_t len,uint8_t** return_buf);

void USBD_Suspend();
void USBD_Resume();


#ifdef __cplusplus
}
#endif



#endif /* GLOBAL_CALLBACKS_H_ */
