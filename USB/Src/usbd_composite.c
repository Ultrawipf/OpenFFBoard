/*
 * Released originally under GPL at (gitlab.com/piotrva/stm32-composite-hid-cdc), modified for OHSC application
 * */


#include <global_callbacks.h>
#include "usbd_composite.h"
#include "usbd_desc.h"
#include "usbd_custom_hid_if.h"
#include "usbd_cdc.h"
#include "usbd_ctlreq.h"
#include "usbd_core.h"

static uint8_t USBD_Composite_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx);

static uint8_t USBD_Composite_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx);

static uint8_t USBD_Composite_Setup (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

static uint8_t USBD_Composite_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t USBD_Composite_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t USBD_Composite_EP0_RxReady (USBD_HandleTypeDef *pdev);

static uint8_t USBD_Composite_EP0_TxSent (USBD_HandleTypeDef *pdev);

static uint8_t *USBD_Composite_GetFSCfgDesc (uint16_t *length);

static uint8_t *USBD_Composite_GetHSCfgDesc (uint16_t *length);

static uint8_t *USBD_Composite_GetOtherSpeedCfgDesc (uint16_t *length);

static uint8_t *USBD_Composite_GetOtherSpeedCfgDesc (uint16_t *length);

static uint8_t *USBD_Composite_GetDeviceQualifierDescriptor (uint16_t *length);

static uint8_t  USBD_Composite_IsoINIncomplete  (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_Composite_IsoOUTIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_Composite_SOF (USBD_HandleTypeDef *pdev);

USBD_ClassTypeDef USBD_Composite =
{
  USBD_Composite_Init,
  USBD_Composite_DeInit,
  USBD_Composite_Setup,
  USBD_Composite_EP0_TxSent, //TODO
  USBD_Composite_EP0_RxReady,
  USBD_Composite_DataIn,
  USBD_Composite_DataOut,
  USBD_Composite_SOF,
  USBD_Composite_IsoINIncomplete, //TODO
  USBD_Composite_IsoOUTIncomplete, //TODO
  USBD_Composite_GetHSCfgDesc,
  USBD_Composite_GetFSCfgDesc,
  USBD_Composite_GetOtherSpeedCfgDesc,
  USBD_Composite_GetDeviceQualifierDescriptor,
};

USBD_ItfTypeDef* USBD_fops[MAX_CLASSES];
void* USBD_pClassData[MAX_CLASSES];
USBD_HandleTypeDef USBD_Handles[MAX_CLASSES];



static int classes;

static USBD_ClassTypeDef *USBD_Classes[MAX_CLASSES];

int in_endpoint_to_class[MAX_ENDPOINTS];

int out_endpoint_to_class[MAX_ENDPOINTS];

uint8_t interface_to_class[16] = {127};

uint8_t *config_descriptor;

uint8_t config_descriptor_buf[CONFIG_DESC_BUF_SIZE];

uint16_t descriptor_size;


static uint8_t USBD_Composite_IsoINIncomplete  (USBD_HandleTypeDef *pdev , uint8_t epnum){
	return USBD_OK;
}

static uint8_t USBD_Composite_IsoOUTIncomplete  (USBD_HandleTypeDef *pdev , uint8_t epnum){
	return USBD_OK;
}
static uint8_t  USBD_Composite_SOF (USBD_HandleTypeDef *pdev){
	USB_SOF();
	return USBD_OK;
}

void USBD_Composite_EPIN_To_Class(uint8_t ep,uint8_t classid){
	in_endpoint_to_class[ep & 0x7F] = classid;
}

void USBD_Composite_EPOUT_To_Class(uint8_t ep,uint8_t classid){
	out_endpoint_to_class[ep & 0x7F] = classid;
}

void USBD_Composite_Set_Descriptor(uint8_t *descriptor, uint16_t size) {
    config_descriptor = descriptor;
    descriptor_size = size;
}

void USBD_InitCompositeDescriptor(USB_ConfigDescType* base_desc){

	for(uint16_t i = 0;i<CONFIG_DESC_BUF_SIZE;i++){
		config_descriptor_buf[i] = 0;
	}
	memcpy(config_descriptor_buf,base_desc,sizeof(USB_ConfigDescType));
	descriptor_size = sizeof(USB_ConfigDescType);
	config_descriptor = (uint8_t*)&config_descriptor_buf;
}

void USBD_AddToCompositeDescriptor(uint8_t* dev, uint16_t len){
	uint16_t len_base = *dev; // first element contains size of header
	memcpy(&config_descriptor_buf[descriptor_size],dev+len_base,len-len_base);
	descriptor_size+=len-len_base;
}


void USBD_Composite_Set_Classes(USBD_ClassTypeDef* class[],uint8_t num,USB_ConfigDescType* base_desc) {
	uint16_t size = 0;
	// Count length of descriptors
	for(uint8_t i = 0;i<num;i++){
		uint16_t ts = 0;
		uint8_t* d = class[i]->GetFSConfigDescriptor(&ts);
		size += (ts - *d);
	}
	// Assign length to base descriptor
	base_desc->wTotalLength = size+sizeof(USB_ConfigDescType);
	USBD_InitCompositeDescriptor(base_desc);

	// Copy class descriptors into composite descriptor
	for(uint8_t i = 0;i<num;i++){
		uint16_t size = 0;
		USBD_Classes[i] = class[i];
		uint8_t* desc = USBD_Classes[i]->GetFSConfigDescriptor(&size);
		USBD_AddToCompositeDescriptor(desc, size);
	}

	classes = num;
}

static uint8_t USBD_Composite_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
  for(int cls = 0; cls < classes; cls++) {
	  uint8_t ret = USBD_Classes[cls]->Init(pdev, cfgidx);
      if (ret != USBD_OK) {
          return USBD_FAIL;
      }

  }

  return USBD_OK;
}

static uint8_t  USBD_Composite_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
  for(int cls = 0; cls < classes; cls++) {
	  uint8_t ret = USBD_Classes[cls]->DeInit(pdev, cfgidx);
      if (ret != USBD_OK) {
          return USBD_FAIL;
      }
  }

  return USBD_OK;
}

static uint8_t USBD_Composite_Setup (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req) {
  uint8_t idx = interface_to_class[req->wIndex & 0x7F];

  switch (req->bmRequest & USB_REQ_TYPE_MASK) {
    case USB_REQ_TYPE_CLASS:
    	//TODO Correct class ids?
    	return USBD_Classes[idx]->Setup(pdev, req);

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest) {

        case USB_REQ_GET_DESCRIPTOR :

          for(int cls = 0; cls < classes; cls++) {
        	  uint8_t ret = USBD_Classes[cls]->Setup(pdev, req);
            if (ret != USBD_OK) {
              return USBD_FAIL;
            }
          }

        break;

		case USB_REQ_GET_INTERFACE :
		case USB_REQ_SET_INTERFACE :
		default:

			return USBD_Classes[idx]->Setup(pdev, req);
		  }
  }
  return USBD_OK;
}

static uint8_t USBD_Composite_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum) {
  int class_index;

  class_index = in_endpoint_to_class[epnum];
  uint8_t ret = USBD_Classes[class_index]->DataIn(pdev, epnum);
  if(ret != USBD_OK){
	  HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin,GPIO_PIN_SET);
  }
  return ret;
}

static uint8_t USBD_Composite_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum) {
  int class_index;

  class_index = out_endpoint_to_class[epnum];

  uint8_t ret = USBD_Classes[class_index]->DataOut(pdev, epnum);
  return ret;

}

static uint8_t USBD_Composite_EP0_RxReady (USBD_HandleTypeDef *pdev) {
  for(int cls = 0; cls < classes; cls++) {
    if (USBD_Classes[cls]->EP0_RxReady != NULL) {
    	uint8_t ret = USBD_Classes[cls]->EP0_RxReady(pdev);
        if (ret != USBD_OK) {
        	return USBD_FAIL;
      }
    }
  }
  return USBD_OK;
}

static uint8_t USBD_Composite_EP0_TxSent (USBD_HandleTypeDef *pdev){
	return USBD_OK;
}

static uint8_t  *USBD_Composite_GetFSCfgDesc (uint16_t *length) {
  *length = descriptor_size;
  return config_descriptor;
}

static uint8_t  *USBD_Composite_GetHSCfgDesc (uint16_t *length) {
  *length = descriptor_size;
  return config_descriptor;
}

static uint8_t  *USBD_Composite_GetOtherSpeedCfgDesc (uint16_t *length) {
  *length = descriptor_size;
  return config_descriptor;
}

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_Composite_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
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

uint8_t  *USBD_Composite_GetDeviceQualifierDescriptor (uint16_t *length) {
  *length = sizeof (USBD_Composite_DeviceQualifierDesc);
  return USBD_Composite_DeviceQualifierDesc;
}


void USBD_Composite_InterfaceToClass(uint8_t ifid, uint8_t clsid){
	interface_to_class[ifid] = clsid;
}


uint8_t  USBD_Composite_RegisterInterface  (USBD_HandleTypeDef   *pdev)
{



	interface_to_class[CDC_INTERFACE] = CDC_IDX;
	interface_to_class[CDC_INTERFACE_DATA] = CDC_IDX;

	USBD_CDC_RegisterInterface(pdev, &USBD_Interface_fops_FS);
	USBD_CUSTOM_HID_RegisterInterface(pdev, &USBD_CustomHID_fops_FS);

	interface_to_class[HID_INTERFACE] = HID_IDX;


	return USBD_OK;
}
