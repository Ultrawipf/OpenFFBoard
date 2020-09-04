
#ifndef __USB_MIDI_CORE_H
#define __USB_MIDI_CORE_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include  "usbd_ioreq.h"

uint16_t MIDI_DataTx(uint8_t *msg, uint16_t length);
#define USB_MIDI_CONFIG_DESC_SIZ       0x65

#define   MIDI_OUT_EP         0x02
#define   MIDI_IN_EP          0x83
#define   MIDI_DATA_IN_PACKET_SIZE  0x40
#define   MIDI_DATA_OUT_PACKET_SIZE 0x40
#define   MIDI_RX_DATA_SIZE 0x64
#define	  MIDI_INTERFACE_A 2
#define	  MIDI_INTERFACE_B 3


#define MIDI_BUF_SIZE 64


extern USBD_ClassTypeDef  USBD_Midi_ClassDriver;

typedef struct _USBD_Midi_Itf
{
  int8_t (* Receive)       (uint8_t *, uint32_t);  

}USBD_Midi_ItfTypeDef;


typedef struct
{
  
  uint8_t rxBuffer[MIDI_BUF_SIZE];
  uint32_t rxLen;
}
USBD_Midi_HandleTypeDef; 


uint8_t  USBD_Midi_RegisterInterface  (USBD_HandleTypeDef   *pdev, 
                                      USBD_Midi_ItfTypeDef *fops);

#ifdef __cplusplus
}
#endif

#endif  /* __USB_MIDI_CORE_H */
