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


#ifdef USE_EEPROM_EMULATION

/**
 * Formats the eeprom emulation section to delete all data
 */
bool Flash_Format(){
	HAL_FLASH_Unlock();
	bool res = (EE_Format() == HAL_OK);
	HAL_FLASH_Lock();
	return res;
}
/**
 * Called after startup
 */
__weak bool Flash_Init(){
	HAL_FLASH_Unlock();
	// Clear all the error flags
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR);
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGAERR);
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGPERR);
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);

	bool state = (EE_Init() == EE_OK);
	HAL_FLASH_Lock();
	return state;
}
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
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGAERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);
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
bool Flash_Read(uint16_t adr,uint16_t *buf,bool checkempty){
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
		// Clear all the error flags
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGAERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGPERR);
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);
		EE_WriteVariable(adr, def);
		HAL_FLASH_Lock();
		return false;
	}
	return true;
}

#elif defined(I2C_PORT_EEPROM)
uint8_t i2cBufferEeprom[sizeof(uint16_t)] = {0};
#include "string.h" // Memcpy
#include "cassert"


bool Flash_Write(uint16_t adr,uint16_t dat){
	uint16_t dataLength = sizeof(dat);
	memcpy(i2cBufferEeprom,&dat, dataLength);
	adr *= sizeof(dat)/I2C_EEPROM_DATA_SIZE;
	adr = I2C_EEPROM_OFS+adr;
	uint16_t curAdr = adr;
	assert(adr < I2C_EEPROM_SIZE);

	// Do segmented writes
	bool res = false;
	while(curAdr < adr+dataLength){
		while(!HAL_I2C_IsDeviceReady(&I2C_PORT_EEPROM, I2C_EEPROM_ADR, 100, I2C_EEPROM_TIMEOUT) == HAL_OK){
			HAL_Delay(1);
		}

		uint16_t wLen = std::min<uint16_t>(dataLength,I2C_EEPROM_PAGEWRITE_SIZE - (adr % I2C_EEPROM_PAGEWRITE_SIZE));
		res = HAL_I2C_Mem_Write(&I2C_PORT_EEPROM, I2C_EEPROM_ADR, curAdr, I2C_EEPROM_ADR_SIZE, i2cBufferEeprom, wLen,I2C_EEPROM_TIMEOUT) == HAL_OK;
		curAdr+=wLen;
		if(!res){
			break;
		}
	}

	return res;
}
bool Flash_ReadWriteDefault(uint16_t adr,uint16_t *buf,uint16_t def){
	if(!Flash_Read(adr,buf)){
		return Flash_Write(adr, def);
	}
	return true;
}
/**
 * Reads a value and if checkempty is true returns false if the read value is the erased value (0xffff) or not found
 */
bool Flash_Read(uint16_t adr,uint16_t *buf, bool checkempty){
	adr *= sizeof(*buf)/I2C_EEPROM_DATA_SIZE;
	assert(adr < I2C_EEPROM_SIZE);
	while(!HAL_I2C_IsDeviceReady(&I2C_PORT_EEPROM, I2C_EEPROM_ADR, 100, I2C_EEPROM_TIMEOUT) == HAL_OK){
		HAL_Delay(1);
	}
	bool res = HAL_I2C_Mem_Read(&I2C_PORT_EEPROM, I2C_EEPROM_ADR, I2C_EEPROM_OFS+adr, I2C_EEPROM_ADR_SIZE, i2cBufferEeprom, 2, I2C_EEPROM_TIMEOUT) == HAL_OK;

	if(checkempty){
		bool empty = true;
		for(uint8_t i = 0;i<sizeof(i2cBufferEeprom);i++){
			if(i2cBufferEeprom[i] != I2C_EEPROM_ERASED){
				empty = false;
				break;
			}
		}
		res = empty ? false : res;
	}
	if(res) // Only copy if success
		memcpy(buf,i2cBufferEeprom,sizeof(i2cBufferEeprom));
	return res;
}

/**
 * Erases the whole EEPROM to its default value
 */
bool Flash_Format(){
	bool flag = true;
	std::array<uint8_t,I2C_EEPROM_PAGEWRITE_SIZE> eraseBuf;
	eraseBuf.fill(I2C_EEPROM_ERASED);
	for(uint32_t i=I2C_EEPROM_OFS;i<I2C_EEPROM_SIZE;i+=I2C_EEPROM_PAGEWRITE_SIZE){

		bool res = HAL_I2C_Mem_Write(&I2C_PORT_EEPROM, I2C_EEPROM_ADR, I2C_EEPROM_OFS+i, I2C_EEPROM_ADR_SIZE, eraseBuf.data(), std::min<int>(I2C_EEPROM_PAGEWRITE_SIZE,I2C_EEPROM_SIZE-i),I2C_EEPROM_TIMEOUT) == HAL_OK;
		if(!res){
			flag = false;
		}else{
			while(!HAL_I2C_IsDeviceReady(&I2C_PORT_EEPROM, I2C_EEPROM_ADR, 100, I2C_EEPROM_TIMEOUT) == HAL_OK){
				HAL_Delay(1);
			}
		}
	}
	return flag;
}
bool Flash_Init(){
	return HAL_I2C_IsDeviceReady(&I2C_PORT_EEPROM, I2C_EEPROM_ADR, 10, I2C_EEPROM_TIMEOUT) == HAL_OK;
}

#else
// No flash mode defined

__weak bool Flash_Write(uint16_t adr,uint16_t dat){
	return true;
}
__weak bool Flash_ReadWriteDefault(uint16_t adr,uint16_t *buf,uint16_t def){
	return true;
}
__weak bool Flash_Read(uint16_t adr,uint16_t *buf){
	return false;
}
__weak bool Flash_Format(){
	return false;
}
__weak bool Flash_Init(){
	return true;
}
#endif
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
