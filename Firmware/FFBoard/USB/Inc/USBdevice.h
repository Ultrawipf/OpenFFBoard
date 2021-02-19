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



/*
 * This class defines a usb device and implements callbacks for getting the basic
 * usb descriptors for tinyusb.
 * Different usb configurations are predefined in usb_descriptors.c TODO
 */
class USBdevice : public cpp_freertos::Thread {
public:
	USBdevice(const tusb_desc_device_t* deviceDesc,const uint8_t (*confDesc),const usb_string_desc_t* strings);
	virtual ~USBdevice();
	void Run(); // Thread loop
	void virtual registerUsb();
	void setHidDesc(const uint8_t* desc); // Sets the pointer to the hid descriptor

	virtual const uint8_t* getUsbDeviceDesc();
	virtual const uint8_t* getUsbConfigurationDesc(uint8_t index);
	virtual uint16_t* getUsbStringDesc(uint8_t index, uint16_t langid);
	virtual std::string getUsbSerial();
	virtual const uint8_t* getHidDesc();

protected:
	const tusb_desc_device_t* desc_device;
	const uint8_t* desc_conf; // Device configuration descriptor
	const usb_string_desc_t* string_desc;
	const uint8_t* hid_desc = nullptr;
};

#endif /* USB_SRC_USBDEVICE_H_ */
