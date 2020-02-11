/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_DEVICE__H__
#define __USB_DEVICE__H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "main.h"
#define COMPOSITE_USB

/** USB Device initialization function. */
void USB_DEVICE_Init(uint8_t composite);


#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE__H__ */
