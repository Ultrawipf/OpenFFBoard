/*
 * UsbHidHandler.h
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#ifndef USBHIDHANDLER_H_
#define USBHIDHANDLER_H_
#include "cppmain.h"
class UsbHidHandler {
public:
	UsbHidHandler();
	virtual ~UsbHidHandler();
	virtual void hidOut(uint8_t* report);
	virtual void hidGet(uint8_t id,uint16_t len,uint8_t** return_buf);
	void registerHidCallback();
};

#endif /* USBHIDHANDLER_H_ */
