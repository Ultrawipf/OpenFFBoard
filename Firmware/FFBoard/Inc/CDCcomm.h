/*
 * CDCcomm.h
 *
 *  Created on: 17.11.2021
 *      Author: Yannick
 */

#ifndef SRC_CDCCOMM_H_
#define SRC_CDCCOMM_H_

#include "tusb.h"
#include "cppmain.h"
#include "semaphore.hpp"

class CDCcomm {
public:

	static uint16_t cdcSend(std::string* reply,uint8_t itf);
	static void cdcFinished(uint8_t itf = 0);
	static uint32_t remainingData(uint8_t itf = 0);
	static void clearRemainingBuffer(uint8_t itf = 0);
	static bool connected(uint8_t itf=0);

private:
	static bool usb_busy_retry;
	static std::string remainingStrs[CFG_TUD_CDC];
	static std::string tString;
	CDCcomm();
	virtual ~CDCcomm();
	static cpp_freertos::BinarySemaphore cdcSems[CFG_TUD_CDC];
};

#endif /* SRC_CDCCOMM_H_ */
