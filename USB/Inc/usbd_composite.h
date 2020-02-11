#ifndef __USB_COMPOSITE_H
#define __USB_COMPOSITE_H

#ifdef __cplusplus
 extern "C" {
#endif
#ifndef PACKED
#if defined (__ICCARM__)
#define PACKED(X) __packed X
#else
#define PACKED(X) X __packed
#endif
#endif

#include "usbd_ioreq.h"
#include "usbd_cdc_if.h"
#include "usbd_custom_hid_if.h"

#define MAX_CLASSES 2

#define MAX_ENDPOINTS 16

#define CONFIG_DESC_BUF_SIZE 256

 typedef struct USBD_ItfTypeDef
 {
   uint8_t                  *pReport;
   int8_t (* Init)          (void);
   int8_t (* DeInit)        (void);
   int8_t (* OutEvent)      (uint8_t, uint8_t );

 }USBD_ItfTypeDef;

 typedef PACKED(struct)
 {
     uint8_t  bLength;               /*!< Size of Descriptor in Bytes */
     uint8_t  bDescriptorType;       /*!< Configuration Descriptor (0x02) */
     uint16_t wTotalLength;          /*!< Total length in bytes of data returned (Auto calculated in USBD_Composite_Set_Classes) */
     uint8_t  bNumInterfaces;        /*!< Number of Interfaces */
     uint8_t  bConfigurationValue;   /*!< Value to use as an argument to select this configuration */
     uint8_t  iConfiguration;        /*!< Index of String Descriptor describing this configuration */
     uint8_t  bmAttributes;          /*!< 0b1[Self Powered][Remote Wakeup]00000 */
     uint8_t  bMaxPower;             /*!< Maximum Power Consumption in 2mA units */
 }USB_ConfigDescType;




extern USBD_HandleTypeDef hUsbDeviceFS; // the usb device



extern USBD_ClassTypeDef USBD_Composite;

extern int in_endpoint_to_class[MAX_ENDPOINTS];

extern int out_endpoint_to_class[MAX_ENDPOINTS];

void USBD_Composite_Set_Descriptor(uint8_t *descriptor, uint16_t size);

void USBD_Composite_Set_Classes(USBD_ClassTypeDef *classes[],uint8_t num,USB_ConfigDescType* base_desc);

void USBD_Composite_Set_fops(USBD_ItfTypeDef *fops0, USBD_ItfTypeDef *fops1);

uint8_t  USBD_Composite_RegisterInterface  (USBD_HandleTypeDef   *pdev);

void USBD_Composite_EPIN_To_Class(uint8_t ep,uint8_t classid);
void USBD_Composite_EPOUT_To_Class(uint8_t ep,uint8_t classid);

void USBD_Composite_InterfaceToClass(uint8_t ifid, uint8_t clsid);

//void USBD_InitCompositeDescriptor(USB_ConfigDescType* base_desc);
//void USBD_AddToCompositeDescriptor(uint8_t* dev, uint16_t len);


#ifdef __cplusplus
}
#endif

#endif
