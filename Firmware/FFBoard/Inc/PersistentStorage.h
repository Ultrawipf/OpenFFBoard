/*
 * PersistentStorage.h
 *
 *  Created on: 26.02.2020
 *      Author: Yannick
 */

#ifndef PERSISTENTSTORAGE_H_
#define PERSISTENTSTORAGE_H_

#include "flash_helpers.h"

// See flash_helpers.h for functions write or read from flash
class PersistentStorage {
public:
	//static std::vector<PersistentStorage*> flashHandlers;

	PersistentStorage();
	virtual ~PersistentStorage();

	virtual void saveFlash(); 		// Write to flash here
	virtual void restoreFlash();	// Load from flash

	static std::vector<PersistentStorage*>& getFlashHandlers() {
		static std::vector<PersistentStorage*> flashHandlers{};
		return flashHandlers;
	}
};

#endif /* PERSISTENTSTORAGE_H_ */
