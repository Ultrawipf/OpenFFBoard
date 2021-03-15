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
#include "hid_cmd_defs.h"


class UsbHidHandler {
public:
	static UsbHidHandler* globalHidHandler;

	UsbHidHandler();
	virtual ~UsbHidHandler();
	virtual void hidOut(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);
	virtual uint16_t hidGet(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);
	void registerHidCallback();

	// HID report descriptor (For gamepad...)
	static void setHidDesc(const uint8_t* desc);
	static const uint8_t* getHidDesc();
	static uint8_t* hid_desc;

};



#endif /* USBHIDHANDLER_H_ */
