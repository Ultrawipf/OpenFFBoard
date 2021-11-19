/*
 * CDCcomm.cpp
 *
 *  Created on: 17.11.2021
 *      Author: Yannick
 *
 *      Contains functions to interface a usb CDC port
 */

#include "CDCcomm.h"
#include "tusb.h"

// Flag to trigger another send call
bool CDCcomm::usb_busy_retry = false;
std::string CDCcomm::remainingStrs[CFG_TUD_CDC] = {""};

CDCcomm::CDCcomm() {
	// TODO Auto-generated constructor stub

}

CDCcomm::~CDCcomm() {
	// TODO Auto-generated destructor stub
}


/**
 * Global callback if cdc transfer is finished. Used to retry a failed transfer
 */
void CDCcomm::cdcFinished(uint8_t itf){
	if(CDCcomm::usb_busy_retry && CDCcomm::remainingStrs[itf].length() > 0){
		cdcSend(&CDCcomm::remainingStrs[itf], itf); // Retry with remaining string
	}
}

/**
 * Sends a string via CDC
 */
uint16_t CDCcomm::cdcSend(std::string* reply,uint8_t itf){

	uint16_t cdc_sent = tud_cdc_n_write(itf,reply->c_str(), std::min<uint16_t>(reply->length(),CFG_TUD_CDC_TX_BUFSIZE));
	tud_cdc_n_write_flush(itf);
	// If we can't write the whole reply copy remainder to send later
	if(cdc_sent < reply->length()){
		remainingStrs[itf].assign(reply->substr(cdc_sent));
		usb_busy_retry = true;
	}else{
		usb_busy_retry = false;
		remainingStrs[itf].clear();
	}
	return cdc_sent;
}
