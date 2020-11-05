/**
  ******************************************************************************
  * @file    usbd_cdc.c
  * @author  MCD Application Team
  * @version V2.4.2
  * @date    11-December-2015
  * @brief   This file provides the high layer firmware functions to manage the 
  *          following functionalities of the USB CDC Class:
  *           - Initialization and Configuration of high and low layer
  *           - Enumeration as CDC Device (and enumeration for each implemented memory interface)
  *           - OUT/IN data transfer
  *           - Command IN transfer (class requests management)
  *           - Error management
  *           
  *  @verbatim
  *      
  *          ===================================================================      
  *                                CDC Class Driver Description
  *          =================================================================== 
  *           This driver manages the "Universal Serial Bus Class Definitions for Communications Devices
  *           Revision 1.2 November 16, 2007" and the sub-protocol specification of "Universal Serial Bus 
  *           Communications Class Subclass Specification for PSTN Devices Revision 1.2 February 9, 2007"
  *           This driver implements the following aspects of the specification:
  *             - Device descriptor management
  *             - Configuration descriptor management
  *             - Enumeration as CDC device with 2 data endpoints (IN and OUT) and 1 command endpoint (IN)
  *             - Requests management (as described in section 6.2 in specification)
  *             - Abstract Control Model compliant
  *             - Union Functional collection (using 1 IN endpoint for control)
  *             - Data interface class
  * 
  *           These aspects may be enriched or modified for a specific user application.
  *          
  *            This driver doesn't implement the following aspects of the specification 
  *            (but it is possible to manage these features with some modifications on this driver):
  *             - Any class-specific aspect relative to communication classes should be managed by user application.
  *             - All communication classes other than PSTN are not managed
  *      
  *  @endverbatim
  *                                  
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"
#include <global_callbacks.h>

/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_CDC 
  * @brief usbd core module
  * @{
  */ 

/** @defgroup USBD_CDC_Private_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup USBD_CDC_Private_Defines
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup USBD_CDC_Private_Macros
  * @{
  */ 

/**
  * @}
  */ 


/** @defgroup USBD_CDC_Private_FunctionPrototypes
  * @{
  */


static uint8_t  USBD_CDC_Init (USBD_HandleTypeDef *pdev, 
                               uint8_t cfgidx);

static uint8_t  USBD_CDC_DeInit (USBD_HandleTypeDef *pdev, 
                                 uint8_t cfgidx);

static uint8_t  USBD_CDC_Setup (USBD_HandleTypeDef *pdev, 
                                USBD_SetupReqTypedef *req);

static uint8_t  USBD_CDC_DataIn (USBD_HandleTypeDef *pdev, 
                                 uint8_t epnum);

static uint8_t  USBD_CDC_DataOut (USBD_HandleTypeDef *pdev, 
                                 uint8_t epnum);

static uint8_t  USBD_CDC_EP0_RxReady (USBD_HandleTypeDef *pdev);

static uint8_t  *USBD_CDC_GetFSCfgDesc (uint16_t *length);

static uint8_t  *USBD_CDC_GetHSCfgDesc (uint16_t *length);

static uint8_t  *USBD_CDC_GetOtherSpeedCfgDesc (uint16_t *length);

static uint8_t  *USBD_CDC_GetOtherSpeedCfgDesc (uint16_t *length);

uint8_t  *USBD_CDC_GetDeviceQualifierDescriptor (uint16_t *length);

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_CDC_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

/**
  * @}
  */ 

/** @defgroup USBD_CDC_Private_Variables
  * @{
  */ 

USBD_CDC_ItfTypeDef *fops_cdc_p;
USBD_CDC_HandleTypeDef     *hcdc;

/* CDC interface class callbacks structure */
USBD_ClassTypeDef  USBD_CDC = 
{
  USBD_CDC_Init,
  USBD_CDC_DeInit,
  USBD_CDC_Setup,
  NULL,                 /* EP0_TxSent, */
  USBD_CDC_EP0_RxReady,
  USBD_CDC_DataIn,
  USBD_CDC_DataOut,
  NULL, // SOF
  NULL,
  NULL,     
  USBD_CDC_GetHSCfgDesc,  
  USBD_CDC_GetFSCfgDesc,    
  USBD_CDC_GetOtherSpeedCfgDesc, 
  USBD_CDC_GetDeviceQualifierDescriptor,
};

/* USB CDC device Configuration Descriptor */

__ALIGN_BEGIN static uint8_t USBD_CDC_OtherSpeedCfgDesc[USB_CDC_CONFIG_DESC_SIZ] __ALIGN_END =
{ 
	0x09,   /* bLength: Configuration Descriptor size */
	USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION,
	USB_CDC_CONFIG_DESC_SIZ,
	0x00,
	0x02,   /* bNumInterfaces: 2 interfaces */
	0x01,   /* bConfigurationValue: */
	0x04,   /* iConfiguration: */
	0xC0,   /* bmAttributes: */
	0x32,   /* MaxPower 100 mA */

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
  	0x00,   /* iInterface: */

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
  	0x00,   /* iInterface: */

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
};

/**
  * @}
  */ 

/** @defgroup USBD_CDC_Private_Functions
  * @{
  */ 

/**
  * @brief  USBD_CDC_Init
  *         Initialize the CDC interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_CDC_Init (USBD_HandleTypeDef *pdev, 
                               uint8_t cfgidx)
{
  uint8_t ret = 0;

  if(pdev->dev_speed == USBD_SPEED_HIGH  ) 
  {  
    /* Open EP IN */
    USBD_LL_OpenEP(pdev,
                   CDC_IN_EP,
                   USBD_EP_TYPE_BULK,
                   CDC_DATA_HS_IN_PACKET_SIZE);
    
    /* Open EP OUT */
    USBD_LL_OpenEP(pdev,
                   CDC_OUT_EP,
                   USBD_EP_TYPE_BULK,
                   CDC_DATA_HS_OUT_PACKET_SIZE);
    
  }
  else
  {
    /* Open EP IN */
    USBD_LL_OpenEP(pdev,
                   CDC_IN_EP,
                   USBD_EP_TYPE_BULK,
                   CDC_DATA_FS_IN_PACKET_SIZE);
    
    /* Open EP OUT */
    USBD_LL_OpenEP(pdev,
                   CDC_OUT_EP,
                   USBD_EP_TYPE_BULK,
                   CDC_DATA_FS_OUT_PACKET_SIZE);
  }
  /* Open Command IN EP */
  USBD_LL_OpenEP(pdev,
                 CDC_CMD_EP,
                 USBD_EP_TYPE_INTR,
                 CDC_CMD_PACKET_SIZE);
  
    
  hcdc = USBD_malloc(sizeof (USBD_CDC_HandleTypeDef));
  pdev->pClassData = hcdc;
  
  if(hcdc == NULL)
  {
    ret = 1; 
  }
  else
  {

    
    /* Init  physical Interface components */
    fops_cdc_p->Init();
    
    /* Init Xfer states */
    hcdc->TxState =0;
    hcdc->RxState =0;
       
    if(pdev->dev_speed == USBD_SPEED_HIGH  ) 
    {      
      /* Prepare Out endpoint to receive next packet */
      USBD_LL_PrepareReceive(pdev,
                             CDC_OUT_EP,
                             hcdc->RxBuffer,
                             CDC_DATA_HS_OUT_PACKET_SIZE);
    }
    else
    {
      /* Prepare Out endpoint to receive next packet */
      USBD_LL_PrepareReceive(pdev,
                             CDC_OUT_EP,
                             hcdc->RxBuffer,
                             CDC_DATA_FS_OUT_PACKET_SIZE);
    }
    
    
  }
  return ret;
}

/**
  * @brief  USBD_CDC_Init
  *         DeInitialize the CDC layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_CDC_DeInit (USBD_HandleTypeDef *pdev, 
                                 uint8_t cfgidx)
{
  uint8_t ret = 0;
  
  /* Open EP IN */
  USBD_LL_CloseEP(pdev,
              CDC_IN_EP);
  
  /* Open EP OUT */
  USBD_LL_CloseEP(pdev,
              CDC_OUT_EP);
  
  /* Open Command IN EP */
  USBD_LL_CloseEP(pdev,
              CDC_CMD_EP);
  
  
  /* DeInit  physical Interface components */
  if(hcdc != NULL)
  {
    fops_cdc_p->DeInit();
    USBD_free(hcdc);
    hcdc = NULL;
  }
  
  return ret;
}

/**
  * @brief  USBD_CDC_Setup
  *         Handle the CDC specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t  USBD_CDC_Setup (USBD_HandleTypeDef *pdev, 
                                USBD_SetupReqTypedef *req)
{

  static uint8_t ifalt = 0;
    
  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
  case USB_REQ_TYPE_CLASS :
    if (req->wLength)
    {
      if (req->bmRequest & 0x80)
      {
        fops_cdc_p->Control(req->bRequest, (uint8_t *)hcdc->data,req->wLength);
          USBD_CtlSendData (pdev, 
                            (uint8_t *)hcdc->data,
                            req->wLength);
      }
      else
      {
        hcdc->CmdOpCode = req->bRequest;
        hcdc->CmdLength = req->wLength;
        
        USBD_CtlPrepareRx (pdev, 
                           (uint8_t *)hcdc->data,
                           req->wLength);
      }
      
    }
    else
    {
      fops_cdc_p->Control(req->bRequest, (uint8_t*)req,0);
    }
    break;

  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {      
    case USB_REQ_GET_INTERFACE :
      USBD_CtlSendData (pdev,
                        &ifalt,
                        1);
      break;
      
    case USB_REQ_SET_INTERFACE :
      break;

    case USB_REQ_GET_DESCRIPTOR:
    	/*
	  if( req->wValue >> 8 == 0x22)
	  {
		uint8_t  *pbuf = USBD_CDC_CfgFSDesc;
		uint16_t len = MIN(USB_CDC_CONFIG_DESC_SIZ , req->wLength);
		USBD_CtlSendData (pdev,	pbuf,len);
	  }


*/
	  break;
    }
 
  default: 
    break;
  }
  return USBD_OK;
}

/**
  * @brief  USBD_CDC_DataIn
  *         Data sent on non-control IN endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  USBD_CDC_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum)
{

  
  if(hcdc != NULL)
  {
	if((hcdc->TxLength) && ((hcdc->TxLength & 63) == 0)){
		hcdc->TxLength = 0;
		USBD_LL_Transmit(pdev, CDC_IN_EP, NULL, 0);
	}
	else{
		hcdc->TxState = 0;
		CDC_Finished();
	}

    return USBD_OK;
  }
  else
  {
    return USBD_FAIL;
  }
}

/**
  * @brief  USBD_CDC_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  USBD_CDC_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum)
{      

  
  /* Get the received data length */
  hcdc->RxLength = USBD_LL_GetRxDataSize (pdev, epnum);
  
  /* USB data will be immediately processed, this allow next USB traffic being 
  NAKed till the end of the application Xfer */
  if(hcdc != NULL)
  {
    fops_cdc_p->Receive(hcdc->RxBuffer, &hcdc->RxLength);

    return USBD_OK;
  }
  else
  {
    return USBD_FAIL;
  }
}



/**
  * @brief  USBD_CDC_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  USBD_CDC_EP0_RxReady (USBD_HandleTypeDef *pdev)
{ 

  
  if((fops_cdc_p != NULL) && (hcdc->CmdOpCode != 0xFF))
  {
    fops_cdc_p->Control(hcdc->CmdOpCode,
                                                      (uint8_t *)hcdc->data,
                                                      hcdc->CmdLength);
      hcdc->CmdOpCode = 0xFF; 
      
  }
  return USBD_OK;
}

/**
  * @brief  USBD_CDC_GetFSCfgDesc 
  *         Return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_CDC_GetFSCfgDesc (uint16_t *length)
{
  *length = sizeof (USBD_CDC_OtherSpeedCfgDesc);
  return USBD_CDC_OtherSpeedCfgDesc;
}

/**
  * @brief  USBD_CDC_GetHSCfgDesc 
  *         Return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_CDC_GetHSCfgDesc (uint16_t *length)
{
  *length = sizeof (USBD_CDC_OtherSpeedCfgDesc);
  return USBD_CDC_OtherSpeedCfgDesc;
}

/**
  * @brief  USBD_CDC_GetCfgDesc 
  *         Return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_CDC_GetOtherSpeedCfgDesc (uint16_t *length)
{
  *length = sizeof (USBD_CDC_OtherSpeedCfgDesc);
  return USBD_CDC_OtherSpeedCfgDesc;
}

/**
* @brief  DeviceQualifierDescriptor 
*         return Device Qualifier descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
uint8_t  *USBD_CDC_GetDeviceQualifierDescriptor (uint16_t *length)
{
  *length = sizeof (USBD_CDC_DeviceQualifierDesc);
  return USBD_CDC_DeviceQualifierDesc;
}

/**
* @brief  USBD_CDC_RegisterInterface
  * @param  pdev: device instance
  * @param  fops: CD  Interface callback
  * @retval status
  */
uint8_t  USBD_CDC_RegisterInterface  (USBD_HandleTypeDef   *pdev, 
                                      USBD_CDC_ItfTypeDef *fops)
{
  uint8_t  ret = USBD_FAIL;
  
  if(fops != NULL)
  {
    fops_cdc_p= fops;
    pdev->pUserData = fops;
    ret = USBD_OK;    
  }
  
  return ret;
}

/**
  * @brief  USBD_CDC_SetTxBuffer
  * @param  pdev: device instance
  * @param  pbuff: Tx Buffer
  * @retval status
  */
uint8_t  USBD_CDC_SetTxBuffer  (USBD_HandleTypeDef   *pdev,
                                uint8_t  *pbuff,
                                uint16_t length)
{

  
  hcdc->TxBuffer = pbuff;
  hcdc->TxLength = length;  
  
  return USBD_OK;  
}


/**
  * @brief  USBD_CDC_SetRxBuffer
  * @param  pdev: device instance
  * @param  pbuff: Rx Buffer
  * @retval status
  */
uint8_t  USBD_CDC_SetRxBuffer  (USBD_HandleTypeDef   *pdev,
                                   uint8_t  *pbuff)
{
  
  hcdc->RxBuffer = pbuff;
  
  return USBD_OK;
}

/**
  * @brief  USBD_CDC_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
uint8_t  USBD_CDC_TransmitPacket(USBD_HandleTypeDef *pdev)
{      

  if(hcdc != NULL)
  {
    if(hcdc->TxState == 0)
    {
      /* Tx Transfer in progress */
      hcdc->TxState = 1;
      
      /* Transmit next packet */
      USBD_LL_Transmit(pdev,
                       CDC_IN_EP,
                       hcdc->TxBuffer,
                       hcdc->TxLength);
      
      return USBD_OK;
    }
    else
    {

      return USBD_BUSY;
    }
  }
  else
  {
    return USBD_FAIL;
  }
}


/**
  * @brief  USBD_CDC_ReceivePacket
  *         prepare OUT Endpoint for reception
  * @param  pdev: device instance
  * @retval status
  */
uint8_t  USBD_CDC_ReceivePacket(USBD_HandleTypeDef *pdev)
{      

  
  /* Suspend or Resume USB Out process */
  if(hcdc != NULL)
  {
    if(pdev->dev_speed == USBD_SPEED_HIGH  ) 
    {      
      /* Prepare Out endpoint to receive next packet */
      USBD_LL_PrepareReceive(pdev,
                             CDC_OUT_EP,
                             hcdc->RxBuffer,
                             CDC_DATA_HS_OUT_PACKET_SIZE);
    }
    else
    {
      /* Prepare Out endpoint to receive next packet */
      USBD_LL_PrepareReceive(pdev,
                             CDC_OUT_EP,
                             hcdc->RxBuffer,
                             CDC_DATA_FS_OUT_PACKET_SIZE);
    }
    return USBD_OK;
  }
  else
  {
    return USBD_FAIL;
  }
}
/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
