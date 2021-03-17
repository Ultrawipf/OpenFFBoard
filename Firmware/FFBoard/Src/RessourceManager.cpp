/*
 * RessourceManager.cpp
 *
 *  Created on: 17.03.2021
 *      Author: Yannick
 */

#include "RessourceManager.h"

cpp_freertos::BinarySemaphore RessourceManager::spiSemaphores[SPI_PORTS] = {
		cpp_freertos::BinarySemaphore(true),
		cpp_freertos::BinarySemaphore(true),
		cpp_freertos::BinarySemaphore(true)
};

/*
 * Do not create this anywhere else. Only a single instance in cppmain.
 */
RessourceManager::RessourceManager() {
	RessourceManager::ressourceManager = this;
}

RessourceManager::~RessourceManager() {

}

cpp_freertos::BinarySemaphore* RessourceManager::getSpiSemaphore(uint8_t port){
	port-=1;
	if(port < SPI_PORTS){
		return &(spiSemaphores[port]);
	}
	return nullptr;
}
