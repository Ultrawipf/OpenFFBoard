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

#include <tuple>
#include <vector>

bool Flash_Write(uint16_t adr,uint16_t dat); // Writes or updates only if changed or missing
bool Flash_Read(uint16_t adr,uint16_t *buf); // returns true if found, false if error
bool Flash_ReadWriteDefault(uint16_t adr,uint16_t *buf,uint16_t def); // returns and writes def if variable is missing
void Flash_Dump(std::vector<std::tuple<uint16_t,uint16_t>> *result);

template<typename TVal>
inline TVal Flash_Read(uint16_t adr, TVal def) {
	uint16_t buf;

	return Flash_Read(adr, &buf)
			? TVal(buf)
			: def;
}

inline uint16_t pack(uint8_t hb, uint8_t lb) {
	return (hb << 8) | lb;
}

inline std::tuple<uint8_t, uint8_t> unpack(uint16_t v) {
	return { v >> 8, v & 0xFF };
}

#endif /* FLASH_HELPERS_H_ */
