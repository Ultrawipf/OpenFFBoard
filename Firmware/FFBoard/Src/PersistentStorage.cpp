/*
 * PersistentStorage.cpp
 *
 *  Created on: 26.02.2020
 *      Author: Yannick
 */

#include "PersistentStorage.h"
#include "global_callbacks.h"

PersistentStorage::PersistentStorage() {
	extern std::vector<PersistentStorage*> flashHandlers;
	addCallbackHandler(&flashHandlers, this);
}

PersistentStorage::~PersistentStorage() {
	extern std::vector<PersistentStorage*> flashHandlers;
	removeCallbackHandler(&flashHandlers, this);
}

void PersistentStorage::saveFlash(){

}
void PersistentStorage::restoreFlash(){

}
