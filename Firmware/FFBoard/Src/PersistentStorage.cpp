/*
 * PersistentStorage.cpp
 *
 *  Created on: 26.02.2020
 *      Author: Yannick
 */

#include "PersistentStorage.h"
#include "global_callbacks.h"

std::vector<PersistentStorage*> PersistentStorage::flashHandlers;

PersistentStorage::PersistentStorage() {
	addCallbackHandler(flashHandlers, this);
}

PersistentStorage::~PersistentStorage() {
	removeCallbackHandler(flashHandlers, this);
}

void PersistentStorage::saveFlash(){

}
void PersistentStorage::restoreFlash(){

}
