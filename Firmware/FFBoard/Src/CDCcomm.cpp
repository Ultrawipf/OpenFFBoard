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
std::string CDCcomm::tString;
cpp_freertos::BinarySemaphore CDCcomm::cdcSems[CFG_TUD_CDC] = {cpp_freertos::BinarySemaphore(true)};

CDCcomm::CDCcomm() {

}

CDCcomm::~CDCcomm() {

}


/**
 * Global callback if cdc transfer is finished. Used to retry a failed transfer
 */
void CDCcomm::cdcFinished(uint8_t itf){
	cdcSems[itf].Give();
	if(CDCcomm::usb_busy_retry && CDCcomm::remainingStrs[itf].length() > 0){
		cdcSend(&CDCcomm::remainingStrs[itf], itf); // Retry with remaining string
	}
	if(CDCcomm::remainingStrs[itf].capacity() > 512){
		CDCcomm::remainingStrs[itf].shrink_to_fit(); // Prevent permanent increase of size if a long command was buffered
	}
}

/**
 * Checks if data is remaining in a buffer to be sent
 */
uint32_t CDCcomm::remainingData(uint8_t itf){
	return CDCcomm::remainingStrs[itf].size();
}

/**
 * Clears a buffer
 */
void CDCcomm::clearRemainingBuffer(uint8_t itf){
	CDCcomm::remainingStrs[itf].clear();
}

bool CDCcomm::connected(uint8_t itf){
	return tud_cdc_n_connected(itf);
}

/**
 * Sends a string via CDC
 * If not everything can be sent it will be buffered for later in a new string
 */
uint16_t CDCcomm::cdcSend(std::string* reply,uint8_t itf){
	if(!tud_ready() || reply->empty()){ // TODO check if connected when gui sets DTR
		return 0;
	}
	cdcSems[itf].Take();
	uint32_t bufferRemaining = tud_cdc_n_write_available(itf);
	uint32_t cdc_sent = tud_cdc_n_write(itf,reply->c_str(), std::min<uint16_t>(reply->length(),bufferRemaining));
	if(!usb_busy_retry) // We did not retransmit so flush now. otherwise TUD will flush if we were in the callback before
		tud_cdc_n_write_flush(itf);

	// If we can't write the whole reply copy remainder to send later
	if(cdc_sent < reply->length()){
		remainingStrs[itf] = reply->substr(cdc_sent);
		usb_busy_retry = true;
		remainingStrs[itf].shrink_to_fit();
	}else{
		usb_busy_retry = false;
		remainingStrs[itf].clear();
	}

	return cdc_sent;
}
