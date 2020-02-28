/*
 * PersistentStorage.h
 *
 *  Created on: 26.02.2020
 *      Author: Yannick
 */

#ifndef PERSISTENTSTORAGE_H_
#define PERSISTENTSTORAGE_H_

#include "flash_helpers.h"
class PersistentStorage {
public:
	PersistentStorage();
	virtual ~PersistentStorage();

	virtual void saveFlash();
	virtual void restoreFlash();
};

#endif /* PERSISTENTSTORAGE_H_ */
