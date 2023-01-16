/*
 * CRC.cpp
 *
 *  Created on: Jan 10, 2023
 *      Author: Yannick
 */

#include "CRC.h"

/**
 * Calculates a crc8 checksum
 */
uint8_t calculateCrc8(std::array<uint8_t,256> &crctable,uint8_t* buf,uint16_t len,uint8_t crc){
	for(uint16_t i = 0;i<len;i++){
		crc = crctable[buf[i] ^ crc];
	}
	return crc;
}

/**
 * Calculates a 16b checksum using a crc16 table on a 8b buffer
 */
uint16_t calculateCrc16_8(std::array<uint16_t,256> &crctable,uint8_t* buf,uint16_t len,uint16_t crc){
	for(uint16_t i = 0;i<len;i++){
		crc = crctable[(((crc >> 8) ^ buf[i]) & 0xFF)] ^ (crc << 8);
	}
	return crc;
}

/**
 * CRC16 with reversed table bytes
 */
uint16_t calculateCrc16_8_rev(std::array<uint16_t,256> &crctable,uint8_t* buf,uint16_t len,uint16_t crc){
	for(uint16_t i = 0;i<len;i++){
		crc = __REVSH(crctable[(((crc >> 8) ^ buf[i]) & 0xFF)]) ^ (crc << 8);
	}
	return crc;
}
