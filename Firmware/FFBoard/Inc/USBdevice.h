/*
 * USBdevice.h
 *
 *  Created on: 19.02.2021
 *      Author: Yannick
 */

#ifndef USB_SRC_USBDEVICE_H_
#define USB_SRC_USBDEVICE_H_

#include "cppmain.h"
#include "tusb.h"
#include "thread.hpp"
#include "usb_descriptors.h"

#define USB_STRING_DESC_BUF_SIZE 32



/**
 * This class defines a usb device and implements callbacks for getting the basic
 * usb descriptors for tinyusb.
 * Different usb configurations are predefined in usb_descriptors.c
 * appendSerial: adds the last n chars of the serial number to the interface strings
 */
class USBdevice : public cpp_freertos::Thread {
public:
	USBdevice(const tusb_desc_device_t* deviceDesc,const uint8_t (*confDesc),const usb_string_desc_t* strings,uint8_t appendSerial = 4);
	virtual ~USBdevice();
	void Run(); // Thread loop
	void virtual registerUsb();

	virtual const uint8_t* getUsbDeviceDesc();
	virtual const uint8_t* getUsbConfigurationDesc(uint8_t index);
	virtual uint16_t* getUsbStringDesc(uint8_t index, uint16_t langid);
	virtual std::string getUsbSerial();

protected:
	const tusb_desc_device_t* desc_device;
	const uint8_t* desc_conf; // Device configuration descriptor
	const usb_string_desc_t* string_desc;
	uint8_t appendSerial;
};

#endif /* USB_SRC_USBDEVICE_H_ */
