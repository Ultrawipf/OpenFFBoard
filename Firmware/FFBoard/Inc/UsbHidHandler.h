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
	static std::vector<UsbHidHandler*> hidCmdHandlers; // called only for custom cmd report ids
	UsbHidHandler();
	virtual ~UsbHidHandler();
	virtual void hidOutCmd(HID_Custom_Data_t* data); // Called when the custom command report is received
	virtual bool sendHidCmd(HID_Custom_Data_t* data);
	virtual void hidOut(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);
	virtual uint16_t hidGet(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);
	void registerHidCallback();
};



#endif /* USBHIDHANDLER_H_ */
