/*
 * ErrorHandler.cpp
 *
 *  Created on: 09.02.2021
 *      Author: Yannick
 */

#include "ErrorHandler.h"
#include "global_callbacks.h"

std::vector<ErrorHandler*> ErrorHandler::errorHandlers;
std::vector<Error_t> ErrorHandler::errors;

ErrorHandler::ErrorHandler() {
	addCallbackHandler(errorHandlers,this);

}

ErrorHandler::~ErrorHandler() {
	removeCallbackHandler(errorHandlers,this);
}

void ErrorHandler::addError(Error_t &error){
	for(Error_t e : errors){
		if(error == e){
			return;
		}
	}
	errors.push_back(error);

	// Call all error handler with this error
	for(ErrorHandler* e : errorHandlers){
		e->errorCallback(error, false);
	}
}

void ErrorHandler::clearError(Error_t &error){
	for (uint8_t i = 0; i < errors.size(); i++){
		if((errors)[i] == error){
			errors.erase(errors.begin()+i);
			break;
		}
	}

	// Call all error handler with this error and set clear flag
	for(ErrorHandler* e : errorHandlers){
		e->errorCallback(error, true);
	}
}

void ErrorHandler::errorCallback(Error_t &error, bool cleared){

}
