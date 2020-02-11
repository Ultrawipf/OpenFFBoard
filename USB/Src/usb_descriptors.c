#include "usb_descriptors.h"

#ifdef false


__ALIGN_BEGIN uint8_t COMPOSITE_CDC_HID_DESCRIPTOR[COMPOSITE_CDC_HID_DESCRIPTOR_SIZE] __ALIGN_END =
{
		/*Configuration Descriptor*/
		0x09,   /* bLength: Configuration Descriptor size */
		USB_DESC_TYPE_CONFIGURATION,      /* bDescriptorType: Configuration */
		COMPOSITE_CDC_HID_DESCRIPTOR_SIZE,                /* wTotalLength:no of returned bytes */
		0x00,
		0x03,   /* bNumInterfaces */
		0x01,   /* bConfigurationValue: Configuration value */
		0x02,   /* iConfiguration: Index of string descriptor describing the configuration */
		0xC0,   /* bmAttributes: self powered */
		0x32,   /* MaxPower 100 mA */


	/*     */
	/* CDC */
	/*     */

	/*---------------------------------------------------------------------------*/
	/*IAD to associate the two CDC interfaces */

	0x08, // bLength: Interface Descriptor size
	0x0B, // bDescriptorType: IAD
	CDC_INTERFACE, // bFirstInterface
	0x02, // bInterfaceCount
	0x02, // bFunctionClass: CDC
	0x02, // bFunctionSubClass
	0x01, // bFunctionProtocol
	0x02, // iFunction


	/*Interface Descriptor */
	0x09,   /* bLength: Interface Descriptor size */
	USB_DESC_TYPE_INTERFACE,  /* bDescriptorType: Interface */
	/* Interface descriptor type */
	CDC_INTERFACE,   /* bInterfaceNumber: Number of Interface */
	0x00,   /* bAlternateSetting: Alternate setting */
	0x01,   /* bNumEndpoints: 1 endpoints used. (3 for single) */
	0x02,   /* bInterfaceClass: Communication Interface Class */
	0x02,   /* bInterfaceSubClass: Abstract Control Model */
	0x01,   /* bInterfaceProtocol: Common AT commands */
	0x02,   /* iInterface: */

	/*Header Functional Descriptor*/
	0x05,   /* bLength: Endpoint Descriptor size */
	0x24,   /* bDescriptorType: CS_INTERFACE */
	0x00,   /* bDescriptorSubtype: Header Func Desc */
	0x10,   /* bcdCDC: spec release number */
	0x01,

	/*Call Management Functional Descriptor*/
	0x05,   /* bFunctionLength */
	0x24,   /* bDescriptorType: CS_INTERFACE */
	0x01,   /* bDescriptorSubtype: Call Management Func Desc */
	0x00,   /* bmCapabilities: D0+D1 */
	0x01,   /* bDataInterface: 1 */

	/*ACM Functional Descriptor*/
	0x04,   /* bFunctionLength */
	0x24,   /* bDescriptorType: CS_INTERFACE */
	0x02,   /* bDescriptorSubtype: Abstract Control Management desc */
	0x02,   /* bmCapabilities */

	/*Union Functional Descriptor*/ //TODO fix interface numbers
	0x05,   /* bFunctionLength */
	0x24,   /* bDescriptorType: CS_INTERFACE */
	0x06,   /* bDescriptorSubtype: Union func desc */
	CDC_INTERFACE,   /* bMasterInterface: Communication class interface */
	CDC_INTERFACE_DATA,   /* bSlaveInterface0: Data Class Interface */

	/*Endpoint 2 Descriptor*/
	0x07,                           /* bLength: Endpoint Descriptor size */
	USB_DESC_TYPE_ENDPOINT,   /* bDescriptorType: Endpoint */
	CDC_CMD_EP,                     /* bEndpointAddress */
	0x03,                           /* bmAttributes: Interrupt */
	LOBYTE(CDC_CMD_PACKET_SIZE),     /* wMaxPacketSize: */
	HIBYTE(CDC_CMD_PACKET_SIZE),
	0x10,                           /* bInterval: */
	/*---------------------------------------------------------------------------*/

	/*Data class interface descriptor*/
	0x09,   /* bLength: Endpoint Descriptor size */
	USB_DESC_TYPE_INTERFACE,  /* bDescriptorType: */
	CDC_INTERFACE_DATA,   /* bInterfaceNumber: Number of Interface */
	0x00,   /* bAlternateSetting: Alternate setting */
	0x02,   /* bNumEndpoints: Two endpoints used */
	0x0A,   /* bInterfaceClass: CDC */
	0x00,   /* bInterfaceSubClass: */
	0x00,   /* bInterfaceProtocol: */
	0x02,   /* iInterface: */

	/*Endpoint OUT Descriptor*/
	0x07,   /* bLength: Endpoint Descriptor size */
	USB_DESC_TYPE_ENDPOINT,      /* bDescriptorType: Endpoint */
	CDC_OUT_EP,                        /* bEndpointAddress */
	0x02,                              /* bmAttributes: Bulk */
	LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),  /* wMaxPacketSize: */
	HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
	0x00,                              /* bInterval: ignore for Bulk transfer */

	/*Endpoint IN Descriptor*/
	0x07,   /* bLength: Endpoint Descriptor size */
	USB_DESC_TYPE_ENDPOINT,      /* bDescriptorType: Endpoint */
	CDC_IN_EP,                         /* bEndpointAddress */
	0x02,                              /* bmAttributes: Bulk */
	LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),  /* wMaxPacketSize: */
	HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
	0x00,                               /* bInterval: ignore for Bulk transfer */

	/*     */
	/* HID */
	/*     */

	/************** Descriptor of CUSTOM HID interface ****************/
	/* 09 */
	0x09,         /*bLength: Interface Descriptor size*/
	USB_DESC_TYPE_INTERFACE,/*bDescriptorType: Interface descriptor type*/
	HID_INTERFACE,         /*bInterfaceNumber: Number of Interface*/
	0x00,         /*bAlternateSetting: Alternate setting*/
	0x02,         /*bNumEndpoints*/
	0x03,         /*bInterfaceClass: CUSTOM_HID*/
	0x00,         /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
	0x00,         /*nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
	2,            /*iInterface: Index of string descriptor (0/1)*/
	/******************** Descriptor of CUSTOM_HID *************************/
	/* 18 */
	0x09,         /*bLength: CUSTOM_HID Descriptor size*/
	CUSTOM_HID_DESCRIPTOR_TYPE, /*bDescriptorType: CUSTOM_HID*/
	0x11,         /*bCUSTOM_HIDUSTOM_HID: CUSTOM_HID Class Spec release number*/
	0x01,
	0x00,         /*bCountryCode: Hardware target country*/
	0x01,         /*bNumDescriptors: Number of CUSTOM_HID class descriptors to follow*/
	0x22,         /*bDescriptorType*/
	USBD_CUSTOM_HID_REPORT_DESC_SIZE&0xff,/*wItemLength: Total length of Report descriptor*/
	USBD_CUSTOM_HID_REPORT_DESC_SIZE>>8,
	/******************** Descriptor of Custom HID endpoints ********************/
	/* 27 */
	0x07,          /*bLength: Endpoint Descriptor size*/
	USB_DESC_TYPE_ENDPOINT, /*bDescriptorType:*/

	CUSTOM_HID_EPIN_ADDR,     /*bEndpointAddress: Endpoint Address (IN)*/
	0x03,          /*bmAttributes: Interrupt endpoint*/
	CUSTOM_HID_EPIN_SIZE, /*wMaxPacketSize: 2 Byte max */
	0x00,
	HID_FS_BINTERVAL,          /*bInterval: Polling Interval*/
	/* 34 */

	0x07,	         /* bLength: Endpoint Descriptor size */
	USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType: */
	CUSTOM_HID_EPOUT_ADDR,  /*bEndpointAddress: Endpoint Address (OUT)*/
	0x03,	/* bmAttributes: Interrupt endpoint */
	CUSTOM_HID_EPOUT_SIZE,	/* wMaxPacketSize: 64 Bytes max  */
	0x00,
	HID_FS_BINTERVAL,	/* bInterval: Polling Interval */
	/* 41 */
};
#endif

