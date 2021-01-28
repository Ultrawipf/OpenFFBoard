/*
 * UsbHidHandler.h
 *
 *  Created on: 12.02.2020
 *      Author: Yannick
 */

#ifndef USBHIDHANDLER_H_
#define USBHIDHANDLER_H_
#include "cppmain.h"
#include "ffb_defs.h"

typedef struct
{
	uint8_t		reportId = HID_ID_CUSTOMCMD; //HID_ID_CUSTOMCMD_FEATURE
	uint8_t		type = 0;
	uint32_t	id = 0;
	uint32_t	data = 0;
} __attribute__((packed)) HID_Custom_Data_t;

class UsbHidHandler {
public:
	UsbHidHandler();
	virtual ~UsbHidHandler();
	virtual void hidOutCmd(HID_Custom_Data_t* data); // Called when the custom command report is received
	virtual void hidOut(uint8_t* report);
	virtual void hidGet(uint8_t id,uint16_t len,uint8_t** return_buf);
	void registerHidCallback();
};



#endif /* USBHIDHANDLER_H_ */
