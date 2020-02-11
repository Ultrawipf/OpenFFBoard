/**
  ******************************************************************************
  * @file    usbd_customhid.c
  * @author  MCD Application Team
  * @version V2.4.2
  * @date    11-December-2015
  * @brief   This file provides the CUSTOM_HID core functions.
  *
  * @verbatim
  *      
  *          ===================================================================      
  *                                CUSTOM_HID Class  Description
  *          =================================================================== 
  *           This module manages the CUSTOM_HID class V1.11 following the "Device Class Definition
  *           for Human Interface Devices (CUSTOM_HID) Version 1.11 Jun 27, 2001".
  *           This driver implements the following aspects of the specification:
  *             - The Boot Interface Subclass
  *             - Usage Page : Generic Desktop
  *             - Usage : Vendor
  *             - Collection : Application 
  *      
  * @note     In HS mode and when the DMA is used, all variables and data structures
  *           dealing with the DMA during the transaction process should be 32-bit aligned.
  *           
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
#include "usbd_customhid.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"


/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_CUSTOM_HID 
  * @brief usbd core module
  * @{
  */ 

/** @defgroup USBD_CUSTOM_HID_Private_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup USBD_CUSTOM_HID_Private_Defines
  * @{
  */ 

/**
  * @}
  */ 


/** @defgroup USBD_CUSTOM_HID_Private_Macros
  * @{
  */ 
/**
  * @}
  */ 
/** @defgroup USBD_CUSTOM_HID_Private_FunctionPrototypes
  * @{
  */


static uint8_t  USBD_CUSTOM_HID_Init (USBD_HandleTypeDef *pdev, 
                               uint8_t cfgidx);

static uint8_t  USBD_CUSTOM_HID_DeInit (USBD_HandleTypeDef *pdev, 
                                 uint8_t cfgidx);

static uint8_t  USBD_CUSTOM_HID_Setup (USBD_HandleTypeDef *pdev, 
                                USBD_SetupReqTypedef *req);

static uint8_t  *USBD_CUSTOM_HID_GetCfgDesc (uint16_t *length);

static uint8_t  *USBD_CUSTOM_HID_GetDeviceQualifierDesc (uint16_t *length);

static uint8_t  USBD_CUSTOM_HID_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t  USBD_CUSTOM_HID_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_CUSTOM_HID_EP0_RxReady (USBD_HandleTypeDef  *pdev);
/**
  * @}
  */ 

/** @defgroup USBD_CUSTOM_HID_Private_Variables
  * @{
  */ 

USBD_CUSTOM_HID_ItfTypeDef *fops_hid_p;
USBD_CUSTOM_HID_HandleTypeDef     *hhid;

USBD_ClassTypeDef  USBD_CUSTOM_HID = 
{
  USBD_CUSTOM_HID_Init,
  USBD_CUSTOM_HID_DeInit,
  USBD_CUSTOM_HID_Setup,
  NULL, /*EP0_TxSent*/  
  USBD_CUSTOM_HID_EP0_RxReady, /*EP0_RxReady*/ /* STATUS STAGE IN */
  USBD_CUSTOM_HID_DataIn, /*DataIn*/
  USBD_CUSTOM_HID_DataOut,
  NULL, /*SOF */
  NULL,
  NULL,      
  USBD_CUSTOM_HID_GetCfgDesc,
  USBD_CUSTOM_HID_GetCfgDesc, 
  USBD_CUSTOM_HID_GetCfgDesc,
  USBD_CUSTOM_HID_GetDeviceQualifierDesc,
};

/* USB CUSTOM_HID device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_CUSTOM_HID_CfgDesc[USB_CUSTOM_HID_CONFIG_DESC_SIZ] __ALIGN_END =
{
  0x09, /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION, /* bDescriptorType: Configuration */
  USB_CUSTOM_HID_CONFIG_DESC_SIZ,
  /* wTotalLength: Bytes returned */
  0x00,
  0x01,         /*bNumInterfaces: 1 interface*/
  0x01,         /*bConfigurationValue: Configuration value*/
  0x00,         /*iConfiguration: Index of string descriptor describing
  the configuration*/
  0xC0,         /*bmAttributes: bus powered */
  0x32,         /*MaxPower 100 mA: this current is used for detecting Vbus*/

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
} ;

/* USB CUSTOM_HID device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_CUSTOM_HID_Desc[USB_CUSTOM_HID_DESC_SIZ] __ALIGN_END =
{
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
};

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_CUSTOM_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
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

/** @defgroup USBD_CUSTOM_HID_Private_Functions
  * @{
  */ 

/**
  * @brief  USBD_CUSTOM_HID_Init
  *         Initialize the CUSTOM_HID interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_CUSTOM_HID_Init (USBD_HandleTypeDef *pdev, 
                               uint8_t cfgidx)
{
  uint8_t ret = 0;

  /* Open EP IN */
  USBD_LL_OpenEP(pdev,
                 CUSTOM_HID_EPIN_ADDR,
                 USBD_EP_TYPE_INTR,
                 CUSTOM_HID_EPIN_SIZE);  
  
  /* Open EP OUT */
  USBD_LL_OpenEP(pdev,
                 CUSTOM_HID_EPOUT_ADDR,
                 USBD_EP_TYPE_INTR,
                 CUSTOM_HID_EPOUT_SIZE);
  
  hhid = USBD_malloc(sizeof (USBD_CUSTOM_HID_HandleTypeDef));
  
  if(hhid == NULL)
  {
    ret = 1; 
  }
  else
  {
    hhid = (USBD_CUSTOM_HID_HandleTypeDef*) hhid;
      
    hhid->state = CUSTOM_HID_IDLE;
    fops_hid_p->Init();
          /* Prepare Out endpoint to receive 1st packet */ 
    USBD_LL_PrepareReceive(pdev, CUSTOM_HID_EPOUT_ADDR, hhid->Report_buf, 
                           USBD_CUSTOMHID_OUTREPORT_BUF_SIZE);
  }
    
  return ret;
}

/**
  * @brief  USBD_CUSTOM_HID_Init
  *         DeInitialize the CUSTOM_HID layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_CUSTOM_HID_DeInit (USBD_HandleTypeDef *pdev, 
                                 uint8_t cfgidx)
{
  /* Close CUSTOM_HID EP IN */
  USBD_LL_CloseEP(pdev,
                  CUSTOM_HID_EPIN_ADDR);
  
  /* Close CUSTOM_HID EP OUT */
  USBD_LL_CloseEP(pdev,
                  CUSTOM_HID_EPOUT_ADDR);
  
  /* FRee allocated memory */
  if(hhid != NULL)
  {
    fops_hid_p->DeInit();
    USBD_free(hhid);
    hhid = NULL;
  }
  return USBD_OK;
}

/**
  * @brief  USBD_CUSTOM_HID_Setup
  *         Handle the CUSTOM_HID specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t  USBD_CUSTOM_HID_Setup (USBD_HandleTypeDef *pdev, 
                                USBD_SetupReqTypedef *req)
{
	uint8_t* get_feature_return_buf;
	uint16_t len = 0;
	uint8_t  *pbuf = NULL;
  //USBD_CUSTOM_HID_HandleTypeDef     *hhid = (USBD_CUSTOM_HID_HandleTypeDef*)hhid;

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
  case USB_REQ_TYPE_CLASS :  
    switch (req->bRequest)
    {
      
      
    case CUSTOM_HID_REQ_SET_PROTOCOL:
      hhid->Protocol = (uint8_t)(req->wValue);
      break;
      
    case CUSTOM_HID_REQ_GET_PROTOCOL:
      USBD_CtlSendData (pdev, 
                        (uint8_t *)&hhid->Protocol,
                        1);    
      break;
      
    case CUSTOM_HID_REQ_SET_IDLE:
      hhid->IdleState = (uint8_t)(req->wValue >> 8);
      break;
      
    case CUSTOM_HID_REQ_GET_IDLE:
      USBD_CtlSendData (pdev, 
                        (uint8_t *)&hhid->IdleState,
                        1);        
      break;      
    
    case CUSTOM_HID_REQ_SET_REPORT:
      hhid->IsReportAvailable = 1;
      USBD_CtlPrepareRx (pdev, hhid->Report_buf, (uint8_t)(req->wLength));
      
      break;

    case CUSTOM_HID_REQ_GET_REPORT: //TODO: check get reports
    	get_feature_return_buf = 0;
    	fops_hid_p->GetEvent(req,&get_feature_return_buf);
    	// user must set return buffer in callback now
    	if(get_feature_return_buf == 0){
    		USBD_CtlError (pdev, req);
		    return USBD_FAIL;
    	}else{
    		USBD_CtlSendData (pdev, get_feature_return_buf, (uint8_t)(req->wLength));
    	}

	break;
    default:
      USBD_CtlError (pdev, req);
      return USBD_FAIL; 
    }
    break;
    
  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR:
      if( req->wValue >> 8 == CUSTOM_HID_REPORT_DESC)
      {
        len = MIN(USBD_CUSTOM_HID_REPORT_DESC_SIZE , req->wLength);
        pbuf =  fops_hid_p->pReport;
      }
      else if( req->wValue >> 8 == CUSTOM_HID_DESCRIPTOR_TYPE)
      {
        pbuf = USBD_CUSTOM_HID_Desc;   
        len = MIN(USB_CUSTOM_HID_DESC_SIZ , req->wLength);
      }
      
      USBD_CtlSendData (pdev, 
                        pbuf,
                        len);
      
      break;
      
    case USB_REQ_GET_INTERFACE :
      USBD_CtlSendData (pdev,
                        (uint8_t *)&hhid->AltSetting,
                        1);
      break;
      
    case USB_REQ_SET_INTERFACE :
      hhid->AltSetting = (uint8_t)(req->wValue);
      break;
    }
  }
  return USBD_OK;
}

/**
  * @brief  USBD_CUSTOM_HID_SendReport 
  *         Send CUSTOM_HID Report
  * @param  pdev: device instance
  * @param  buff: pointer to report
  * @retval status
  */
uint8_t USBD_CUSTOM_HID_SendReport     (USBD_HandleTypeDef  *pdev, 
                                 uint8_t *report,
                                 uint16_t len)
{
  //USBD_CUSTOM_HID_HandleTypeDef     *hhid = (USBD_CUSTOM_HID_HandleTypeDef*)hhid;
  if (pdev->dev_state == USBD_STATE_CONFIGURED )
  {
    if(hhid->state == CUSTOM_HID_IDLE)
    {
      hhid->state = CUSTOM_HID_BUSY;
      USBD_LL_Transmit (pdev, 
                        CUSTOM_HID_EPIN_ADDR,                                      
                        report,
                        len);
    }
  }
  return USBD_OK;
}

//void USBD_CUSTOM_HID_SetFeatureGetReport(uint8_t *report){
//	get_feature_return_buf = report;
//}
/**
  * @brief  USBD_CUSTOM_HID_GetCfgDesc 
  *         return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_CUSTOM_HID_GetCfgDesc (uint16_t *length)
{
  *length = sizeof (USBD_CUSTOM_HID_CfgDesc);
  return USBD_CUSTOM_HID_CfgDesc;
}

/**
  * @brief  USBD_CUSTOM_HID_DataIn
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t  USBD_CUSTOM_HID_DataIn (USBD_HandleTypeDef *pdev, 
                              uint8_t epnum)
{
  
  /* Ensure that the FIFO is empty before a new transfer, this condition could 
  be caused by  a new transfer before the end of the previous transfer */
  ((USBD_CUSTOM_HID_HandleTypeDef *)hhid)->state = CUSTOM_HID_IDLE;

  return USBD_OK;
}

/**
  * @brief  USBD_CUSTOM_HID_DataOut
  *         handle data OUT Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t  USBD_CUSTOM_HID_DataOut (USBD_HandleTypeDef *pdev, 
                              uint8_t epnum)
{
  
  //USBD_CUSTOM_HID_HandleTypeDef     *hhid = (USBD_CUSTOM_HID_HandleTypeDef*)hhid;
  
  fops_hid_p->OutEvent((uint8_t*)&hhid->Report_buf);
    
  USBD_LL_PrepareReceive(pdev, CUSTOM_HID_EPOUT_ADDR , hhid->Report_buf, 
                         USBD_CUSTOMHID_OUTREPORT_BUF_SIZE);

  return USBD_OK;
}

/**
  * @brief  USBD_CUSTOM_HID_EP0_RxReady
  *         Handles control request data.
  * @param  pdev: device instance
  * @retval status
  */
uint8_t USBD_CUSTOM_HID_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  //USBD_CUSTOM_HID_HandleTypeDef     *hhid = (USBD_CUSTOM_HID_HandleTypeDef*)hhid;

  if (hhid->IsReportAvailable == 1)
  {
    fops_hid_p->OutEvent((uint8_t*)&hhid->Report_buf);
    hhid->IsReportAvailable = 0;      
  }

  return USBD_OK;
}

/**
* @brief  DeviceQualifierDescriptor 
*         return Device Qualifier descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
static uint8_t  *USBD_CUSTOM_HID_GetDeviceQualifierDesc (uint16_t *length)
{
  *length = sizeof (USBD_CUSTOM_HID_DeviceQualifierDesc);
  return USBD_CUSTOM_HID_DeviceQualifierDesc;
}

/**
* @brief  USBD_CUSTOM_HID_RegisterInterface
  * @param  pdev: device instance
  * @param  fops: CUSTOMHID Interface callback
  * @retval status
  */
uint8_t  USBD_CUSTOM_HID_RegisterInterface  (USBD_HandleTypeDef   *pdev, 
                                             USBD_CUSTOM_HID_ItfTypeDef *fops)
{
  uint8_t  ret = USBD_FAIL;
  
  if(fops != NULL)
  {
    fops_hid_p= fops;
    ret = USBD_OK;    
  }
  
  return ret;
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
