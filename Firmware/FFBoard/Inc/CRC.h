/*
 * CRC.h
 *
 *  Created on: 10.01.2023
 *      Author: Yannick
 */

#ifndef SRC_CRC_H_
#define SRC_CRC_H_
#include "main.h"
#include "array"


template<typename T>
T reverseBits(T value)
{
	static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "Type must be 1,2 or 4 bytes long");
	value = ((value & 0xAAAAAAAA) >> 1) | ((value & 0x55555555) << 1);
	value = ((value & 0xCCCCCCCC) >> 2) | ((value & 0x33333333) << 2);

	if(sizeof(T)>1)
		value = ((value & 0xF0F0F0F0) >> 4) | ((value & 0x0F0F0F0F) << 4);

	if(sizeof(T)>3)
		value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);

	value = (value >> (4*sizeof(T))) | (value << (4*sizeof(T)));

	return value; // & (1 << (sizeof(T)*8)) - 1
}

/**
 * Helper function to create a CRC table of different sizes and types
 */
template<typename T,size_t LEN>
void makeCrcTable(std::array<T,LEN> &table, const T crcpoly,const uint8_t bits,const bool refin = false,const bool refout = false)
{
	T mask = 1 << (bits-1);
	for (uint16_t byte = 0; byte < LEN; ++byte)
	{
		uint8_t shiftin = std::max<int16_t>(bits-8,0);
		T crc = (refin ? reverseBits<T>(byte) >> shiftin : byte);
		for (uint8_t bit = 0; bit < bits; ++bit)
		{
			if (crc & mask)
			{
				crc = (crc << 1) ^ crcpoly;
			}
			else
			{
				crc <<= 1;
			}
		}
		table[byte] =  (refout ? reverseBits<T>(crc) : crc);
	}
}


uint8_t calculateCrc8(std::array<uint8_t,256> &crctable,uint8_t* buf,uint16_t len,uint8_t crc=0);
uint16_t calculateCrc16_8(std::array<uint16_t,256> &crctable,uint8_t* buf,uint16_t len,uint16_t crc=0);
uint16_t calculateCrc16_8_rev(std::array<uint16_t,256> &crctable,uint8_t* buf,uint16_t len,uint16_t crc=0);

#endif /* SRC_CRC_H_ */
