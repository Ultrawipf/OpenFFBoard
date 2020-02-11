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

extern "C" {
#endif

void CDC_Callback(uint8_t* Buf, uint32_t *Len);
void USBD_OutEvent_HID(uint8_t* report);
void USB_SOF();
void USBD_GetEvent_HID(uint8_t id,uint16_t len,uint8_t** return_buf);

#ifdef __cplusplus
}
#endif



#endif /* GLOBAL_CALLBACKS_H_ */
