/*
 * flash_helpers.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */
#include "flash_helpers.h"
#include "eeprom_addresses.h"
#include <vector>
#include "mutex.hpp"

cpp_freertos::MutexStandard flashMutex;
// Flash helpers

// TODO sometimes on F4 chips when writing HAL_FLASH_ERROR_PGS and HAL_FLASH_ERROR_PGP occur and it is not writing

/*
 * Writes a variable to eeprom emulation adr
 * Returns true on success or false if variable is the same or error
 */
bool Flash_Write(uint16_t adr,uint16_t dat){
	//flashMutex.Lock();
	uint16_t buf;
	uint16_t readRes = EE_ReadVariable(adr, &buf);
	bool res = false;
	if(readRes == 1 || (readRes == 0 && buf != dat) ){ // Only write if var updated
		HAL_FLASH_Unlock();
		#ifndef HW_ESP32SX
		// Clear all the error flags
	    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
	    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR);
	    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
	    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGAERR);
	    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGPERR);
	    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);
		#endif
		if(EE_WriteVariable(adr, dat) == HAL_OK){
			res = true;
		}
		HAL_FLASH_Lock();

	}
	//flashMutex.Unlock();
	return res;

}

/*
 * Reads a variable from eeprom emulation and returns true on success
 */
bool Flash_Read(uint16_t adr,uint16_t *buf){
	//flashMutex.Lock();
	bool res = EE_ReadVariable(adr, buf) == 0;
	//flashMutex.Unlock();
	return res;
}

/*
 * Reads a variable or if it does not exist default is written
 */
bool Flash_ReadWriteDefault(uint16_t adr,uint16_t *buf,uint16_t def){
	if(EE_ReadVariable(adr, buf) != 0){
		*buf = def;
		HAL_FLASH_Unlock();
		#ifndef HW_ESP32SX
		// Clear all the error flags
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGAERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);
		#endif
		EE_WriteVariable(adr, def);
		HAL_FLASH_Lock();
		return false;
	}
	return true;
}

/*
 * Dumps all set variables from flash to a vector
 * does by default not include certain calibration constants that could break something if imported on a different board
 */
void Flash_Dump(std::vector<std::tuple<uint16_t,uint16_t>> *result,bool includeAll){
	uint16_t amount = NB_EXPORTABLE_ADR;
	const uint16_t* list = exportableFlashAddresses;
	if(includeAll){
		amount = NB_OF_VAR;
		list = VirtAddVarTab;
	}
	//extern uint16_t VirtAddVarTab[NB_OF_VAR];
	//std::vector<std::pair<uint16_t,uint16_t>> result;

	for(uint32_t i = 0;i<amount ; i++){
		uint16_t v;
		uint16_t adr = list[i];

		if(Flash_Read(adr,&v)){
			result->push_back(std::tuple<uint16_t,uint16_t>{adr,v});
		}

	}
}
