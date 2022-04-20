/*
 * PersistentStorage.cpp
 *
 *  Created on: 26.02.2020
 *      Author: Yannick
 */

#include "PersistentStorage.h"
#include "global_callbacks.h"

//std::vector<PersistentStorage*> PersistentStorage::flashHandlers;

PersistentStorage::PersistentStorage() {
	addCallbackHandler(getFlashHandlers(), this);
}

PersistentStorage::~PersistentStorage() {
	removeCallbackHandler(getFlashHandlers(), this);
}

/**
 * Called when the user uses the "save" command and presses the save button in the configurator
 * Automatically called by the command parser thread for every class that inherits from PersistentStorage
 */
void PersistentStorage::saveFlash(){

}
/**
 * Should be used to restore settings at startup.
 * This is not automatically called and should be called when appropriate.
 * Normally this is done in the constructor of the class
 */
void PersistentStorage::restoreFlash(){

}
