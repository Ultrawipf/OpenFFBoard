/*
 * flash_helpers.h
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#ifndef FLASH_HELPERS_H_
#define FLASH_HELPERS_H_
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#include "eeprom.h"


#ifdef __cplusplus
}
#endif

bool Flash_Write(uint16_t adr,uint16_t dat); // Writes or updates only if changed or missing
bool Flash_Read(uint16_t adr,uint16_t *buf); // returns true if found, false if error
bool Flash_ReadWriteDefault(uint16_t adr,uint16_t *buf,uint16_t def); // returns and writes def if variable is missing



#endif /* FLASH_HELPERS_H_ */
