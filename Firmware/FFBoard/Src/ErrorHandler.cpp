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
std::vector<Error> ErrorHandler::errors;


std::string Error::toString(){
	std::string r = std::to_string((uint32_t)code) + ":";
	switch(type){
		case ErrorType::warning:
			r += "warn";
		break;

		case ErrorType::critical:
			r += "crit";
		break;
		case ErrorType::temporary:
			r += "info";
		break;
		default:
			r += "err";
	}
	r += ":" + (info == "" ? "no info" : info);
	return r;
}


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

void ErrorHandler::addError(Error error){
	for(Error e : errors){
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

void ErrorHandler::clearError(Error error){
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

std::vector<Error>* ErrorHandler::getErrors(){
	return &errors;
}

void ErrorHandler::errorCallback(Error &error, bool cleared){

}

ErrorPrinter::ErrorPrinter() : Thread("errprint",128,10){
	this->Start();
}

void ErrorPrinter::Run(){
	while(1){
		for(Error e : this->errorsToPrint){
			FFBoardMain::sendSerial("Err", e.toString());
		}
		this->errorsToPrint.clear();
		this->Suspend();
	}
}

// TODO prints in thread when called from isr
void ErrorPrinter::errorCallback(Error &error, bool cleared){
	if(!cleared){
		if(isIrq()){
			this->errorsToPrint.push_back(error);
			this->ResumeFromISR();
		}else{
			FFBoardMain::sendSerial("Err", error.toString());
		}
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
