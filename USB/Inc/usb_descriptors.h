#ifndef __USB_DESCRIPTORS_H
#define __USB_DESCRIPTORS_H

#include "usbd_composite.h"
#include "usbd_desc.h"
#include "usbd_custom_hid_if.h"
#include "usbd_cdc.h"
#include "usbd_ctlreq.h"

#ifndef PACKED
#if defined (__ICCARM__)
#define PACKED(X) __packed X
#else
#define PACKED(X) X __packed
#endif
#endif

#define COMPOSITE_CDC_HID_DESCRIPTOR_SIZE 107//99//90//85
#define CDC_INTERFACE 0x00
#define CDC_INTERFACE_DATA 0x01


#define CDC_IDX 0
#define HID_IDX 1





extern uint8_t COMPOSITE_CDC_HID_DESCRIPTOR[COMPOSITE_CDC_HID_DESCRIPTOR_SIZE];

#endif
