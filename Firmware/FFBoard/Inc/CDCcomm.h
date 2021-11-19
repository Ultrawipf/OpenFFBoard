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

class CDCcomm {
public:

	static uint16_t cdcSend(std::string* reply,uint8_t itf);
	static void cdcFinished(uint8_t itf = 0);

private:
	static bool usb_busy_retry;
	static std::string remainingStrs[CFG_TUD_CDC];
	CDCcomm();
	virtual ~CDCcomm();
};

#endif /* SRC_CDCCOMM_H_ */
