/*
 * RessourceManager.h
 *
 *  Created on: 17.03.2021
 *      Author: Yannick
 */

#ifndef SRC_RESSOURCEMANAGER_H_
#define SRC_RESSOURCEMANAGER_H_

#include "semaphore.hpp"

#define SPI_PORTS 3
/*
 * Manages mutexes for ports
 */
class RessourceManager {
public:
	RessourceManager();
	virtual ~RessourceManager();

	static cpp_freertos::BinarySemaphore* getSpiSemaphore(uint8_t port);

	RessourceManager* ressourceManager;

	static cpp_freertos::BinarySemaphore spiSemaphores[SPI_PORTS];

};

#endif /* SRC_RESSOURCEMANAGER_H_ */
