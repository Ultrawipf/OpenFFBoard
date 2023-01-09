/*
 * MotorSimplemotion.cpp
 *
 *  Created on: Jan 9, 2023
 *      Author: Yannick
 */

#include "target_constants.h"
#ifdef SIMPLEMOTION
#include "MotorSimplemotion.h"

bool MotorSimplemotion::crcTableInitialized = false;
uint8_t MotorSimplemotion::tableCRC8[256] __attribute__((section (".ccmram")));

MotorSimplemotion::MotorSimplemotion(uint8_t address) {
	//Init CRC table at runtime to save flash
	if(!crcTableInitialized){ // Generate a CRC8 table the first time an instance is created
		for (uint8_t byte = 0; byte < 256; ++byte)
		  {
		    uint8_t crc = byte;
		    for (uint8_t bit = 0; bit < 8; bit++)
		    {
		      if (crc & 0x80)
		      {
		        crc = (crc << 1) ^ crcpoly;
		      }
		      else
		      {
		        crc <<= 1;
		      }
		    }
		    tableCRC8[byte] = crc;
		  }
		crcTableInitialized = true;
	}

}

MotorSimplemotion::~MotorSimplemotion() {
	// TODO Auto-generated destructor stub
}

void MotorSimplemotion::turn(int16_t power){

}

Encoder* MotorSimplemotion::getEncoder(){
	return static_cast<Encoder>(this);
}

#endif
