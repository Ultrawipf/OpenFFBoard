/*
 * ErrorHandler.cpp
 *
 *  Created on: 09.02.2021
 *      Author: Yannick
 */

#include "ErrorHandler.h"
#include "global_callbacks.h"
#include "FFBoardMain.h"
#include "cppmain.h"
#include "critical.hpp"
#include <span>

std::vector<ErrorHandler*> ErrorHandler::errorHandlers;
//std::vector<Error> ErrorHandler::errors;
std::array<Error,ERRORHANDLER_MAXERRORS> ErrorHandler::errors;


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


ErrorHandler::ErrorHandler(){
//	errors.reserve(10);
	addCallbackHandler(errorHandlers,this);
}

ErrorHandler::~ErrorHandler() {
	removeCallbackHandler(errorHandlers,this);
}

/*
 * Clears ALL error conditions
 */
void ErrorHandler::clearAll(){
	// Call all error handlers
	for(ErrorHandler* e : errorHandlers){
		for(Error& error : errors)
			e->errorCallback(error, true);
	}
	errors.fill(Error());
}

/*
 * Clears errors marked as temporary
 */
void ErrorHandler::clearTemp(){
	for (uint8_t i = 0; i < errors.size(); i++){
		if((errors)[i].type == ErrorType::temporary){
			//errors.erase(errors.begin()+i);
			errors[i] = Error(); // Empty
			break;
		}
	}
	sortErrors();
}

void ErrorHandler::addError(const Error &error){
	for(Error e : errors){
		if(error == e){
			return;
		}
	}
//	errors.push_back(error);
	auto errIt = std::find_if(errors.begin(), errors.end(), [](const Error& err){return !err.isError();});
	if(errIt != errors.end()){
		*errIt = error; // If buffer is full do not store error but still call callbacks.
	}


	// Call all error handler with this error
	//cpp_freertos::CriticalSection::SuspendScheduler();
	for(ErrorHandler* e : errorHandlers){
		e->errorCallback(error, false);
	}
	//cpp_freertos::CriticalSection::ResumeScheduler();
}

void ErrorHandler::clearError(const Error &error){
	for (uint8_t i = 0; i < errors.size(); i++){
		if(errors[i] == error){
//			errors.erase(errors.begin()+i);
			errors[i] = Error(); // Empty
			break;
		}
	}

	// Call all error handler with this error and set clear flag
	for(ErrorHandler* e : errorHandlers){
		e->errorCallback(error, true);
	}
	sortErrors();
}

/*
 * Clears all errors with a specific errorcode
 */
void ErrorHandler::clearError(ErrorCode errorcode){
	for (uint8_t i = 0; i < errors.size(); i++){
		if(errors[i].code == errorcode){
//			errors.erase(errors.begin()+i);
			errors[i] = Error(); // Empty
			for(ErrorHandler* e : errorHandlers){
				e->errorCallback(errors[i], true);
			}
		}
	}
	sortErrors();
}

std::span<Error> ErrorHandler::getErrors(){
	return std::span<Error>(errors.begin(), std::find_if(errors.begin(), errors.end(), [](const Error& err){return err.type == ErrorType::none;}));
}

void ErrorHandler::errorCallback(const Error &error, bool cleared){

}

void ErrorHandler::sortErrors(){
	auto errSortFun = [](const Error& a, const Error& b){
		return a.code < b.code;
	};
	std::sort(errors.begin(),errors.end(),errSortFun);
}


//ClassIdentifier ErrorPrinter::info = {
//	.name = "Errorprinter",
//	.id=CLSID_ERRORS
//};

//ClassIdentifier ErrorPrinter::getInfo(){
//	return info;
//}

ErrorPrinter::ErrorPrinter() : Thread("errprint",256,19){ // Higher than default task but low.
	this->Start();
}

void ErrorPrinter::Run(){
	while(1){
		if(SystemCommands::systemCommandsInstance && enabled){
			std::vector<CommandReply> replies;
			SystemCommands::systemCommandsInstance->replyErrors(replies);
			CommandInterface::broadcastCommandReplyAsync(replies, SystemCommands::systemCommandsInstance, (uint32_t)FFBoardMain_commands::errors, CMDtype::get);

			ErrorHandler::clearTemp(); // Errors are sent. clear them
		}
		this->Suspend();
	}
}


// TODO prints in thread when called from isr
void ErrorPrinter::errorCallback(const Error &error, bool cleared){
	if(!cleared){
//		this->errorsToPrint.push_back(error); // Errors are stored in errorhandler
		if(inIsr()){
			this->ResumeFromISR();
		}else{
			this->Resume();
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
