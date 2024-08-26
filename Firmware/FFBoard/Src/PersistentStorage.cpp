/*
 * PersistentStorage.cpp
 *
 *  Created on: 26.02.2020
 *      Author: Yannick
 */

#include "PersistentStorage.h"
#include "global_callbacks.h"

//std::vector<PersistentStorage*> PersistentStorage::flashHandlers;
bool PersistentStorage::startupComplete = false;

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
 * Should be implemented to restore settings.
 * This is not automatically called and should be called when appropriate.
 */
void PersistentStorage::restoreFlash(){

}

/**
 * Should be used to restore settings after startup if class is created before startup finished.
 * This is preferred to calling restoreFlash directly and calls restoreFlash after startup is finished
 * This is not automatically called and should be called when appropriate.
 * Normally this is done in the constructor of the class
 */
void PersistentStorage::restoreFlashDelayed(){
	if(PersistentStorage::startupComplete){
		restoreFlash();
	}else{
		this->restoreDelayedFlag = true;
	}
}

/**
 * Called after storage has been initialized
 * Will call restoreFlash on any storage handler that is currently waiting
 */
void PersistentStorage::restoreFlashStartupCb(){
	PersistentStorage::startupComplete = true;
	for(PersistentStorage* cls : getFlashHandlers()){
		if(cls->restoreDelayedFlag){
			cls->restoreFlash();
			cls->restoreDelayedFlag = false;
		}
	}
}
