/*
 * ErrorHandler.cpp
 *
 *  Created on: 09.02.2021
 *      Author: Yannick
 */

#include "ErrorHandler.h"
#include "global_callbacks.h"
#include "FFBoardMain.h"

std::vector<ErrorHandler*> ErrorHandler::errorHandlers;
std::vector<Error_t> ErrorHandler::errors;

/*
 * Errors are equivalent if their code and type match
 */
bool operator ==(const Error_t &a, const Error_t &b){return (a.code == b.code && a.type == b.type && a.info == b.info);}

ErrorHandler::ErrorHandler() {
	addCallbackHandler(errorHandlers,this);
}

ErrorHandler::~ErrorHandler() {
	removeCallbackHandler(errorHandlers,this);
}

/*
 * Clears ALL error conditions
 */
void ErrorHandler::clearAll(){
	errors.clear();
}

/*
 * Clears errors marked as temporary
 */
void ErrorHandler::clearTemp(){
	for (uint8_t i = 0; i < errors.size(); i++){
		if((errors)[i].type == ErrorType::temporary){
			errors.erase(errors.begin()+i);
			break;
		}
	}
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
		if(errors[i] == error){
			errors.erase(errors.begin()+i);
			break;
		}
	}

	// Call all error handler with this error and set clear flag
	for(ErrorHandler* e : errorHandlers){
		e->errorCallback(error, true);
	}
}

/*
 * Clears all errors with a specific errorcode
 */
void ErrorHandler::clearError(ErrorCode errorcode){
	for (uint8_t i = 0; i < errors.size(); i++){
		if(errors[i].code == errorcode){
			errors.erase(errors.begin()+i);
			for(ErrorHandler* e : errorHandlers){
				e->errorCallback(errors[i], true);
			}
		}
	}
}

std::vector<Error_t>* ErrorHandler::getErrors(){
	return &errors;
}

void ErrorHandler::errorCallback(Error_t &error, bool cleared){

}



void ErrorPrinter::errorCallback(Error_t &error, bool cleared){
	if(!cleared){
		FFBoardMain::sendSerial("Err", error.toString());
	}
}

void ErrorPrinter::setEnabled(bool enabled){
	if(this->enabled == enabled){
		return;
	}

	if(!enabled){
		removeCallbackHandler(errorHandlers,static_cast<ErrorHandler*>(this));
	}else{
		addCallbackHandler(errorHandlers,static_cast<ErrorHandler*>(this));
	}
	this->enabled = enabled;
}

bool ErrorPrinter::getEnabled(){
	return this->enabled;
}
