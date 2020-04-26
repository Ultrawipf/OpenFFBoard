/*
 * flash_helpers.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */
#include "flash_helpers.h"

// Flash helpers

/*
 * Writes a variable to eeprom emulation adr
 * Returns true on success or false if variable is the same or error
 */
bool Flash_Write(uint16_t adr,uint16_t dat){
	uint16_t buf;
	uint16_t readRes = EE_ReadVariable(adr, &buf);
	if(readRes == 1 || (readRes == 0 && buf != dat) ){ // Only write if var updated
		HAL_FLASH_Unlock();
		EE_WriteVariable(adr, dat);
		HAL_FLASH_Lock();
		return true;
	}
	return false;
}

/*
 * Reads a variable from eeprom emulation and returns true on success
 */
bool Flash_Read(uint16_t adr,uint16_t *buf){
	return(EE_ReadVariable(adr, buf) == 0);
}

/*
 * Reads a variable or if it does not exist default is written
 */
bool Flash_ReadWriteDefault(uint16_t adr,uint16_t *buf,uint16_t def){
	if(EE_ReadVariable(adr, buf) != 0){
		*buf = def;
		HAL_FLASH_Unlock();
		EE_WriteVariable(adr, def);
		HAL_FLASH_Lock();
		return false;
	}
	return true;
}
