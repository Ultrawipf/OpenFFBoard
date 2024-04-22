/*
 * ErrorHandler.h
 *
 *  Created on: 09.02.2021
 *      Author: Yannick
 */

#ifndef SRC_ERRORHANDLER_H_
#define SRC_ERRORHANDLER_H_
#include <array>
#include <string>
#include "thread.hpp"
#include "CommandHandler.h"
#include <span>

#ifndef ERRORHANDLER_MAXERRORS
#define ERRORHANDLER_MAXERRORS 16
#endif

/*
 * Error code definitions
 */

enum class ErrorCode : uint32_t{
			none = 0,
			shutdown = 1,
			emergencyStop = 2,
			brakeResistorFailure = 3,
			systemError = 5,

			cmdNotFound = 5,
			cmdExecutionError = 6,

			undervoltage = 10,
			overvoltage = 11,
			tmcCommunicationError = 12,
			tmcPLLunlocked = 13,

			overtemp = 15,

			encoderAlignmentFailed = 20,
			adcCalibrationError = 21,
			tmcCalibFail = 22,
			encoderIndexMissed = 23,
			encoderReversed = 24,

			axisOutOfRange = 31,

			externalConfigurationError = 32
};
/*
 * Error type for severity
 * temporary errors are cleared when read back
 * critical errors cause a shutdown or have to be cleared externally
 */
enum class ErrorType : uint8_t{
	none,warning,critical,temporary
};

class Error{
public:
	//Error(const Error &e);
	Error(){}; // No error
	Error(ErrorCode code, ErrorType type, std::string info) : code(code), type(type), info(info){};
	ErrorCode code = ErrorCode::none;
	ErrorType type = ErrorType::none;
	std::string info;

	std::string toString();

	/*
	 * Errors are equivalent if their code and type match
	 */
	bool operator ==(const Error &b) const {return (this->code == b.code && this->type == b.type);}
	bool isError() const {return this->code != ErrorCode::none && this->type != ErrorType::none;}
};




class ErrorHandler {
public:
	static std::vector<ErrorHandler*> errorHandlers;

	ErrorHandler();
	virtual ~ErrorHandler();
	virtual void errorCallback(const Error &error, bool cleared); // Called when a new error happened. Warning: Could be called from ISR!

	static void addError(const Error &error);
	static void clearError(const Error &error);
	static void clearError(ErrorCode errorcode);
	static void clearTemp();
	static void clearAll();

	static std::span<Error> getErrors(); // Returns a vector of active errors

protected:
	static std::array<Error,ERRORHANDLER_MAXERRORS> errors;
	static void sortErrors();
};

/*
 * Can handle errors occuring in the main class
 */
class ErrorPrinter : ErrorHandler, cpp_freertos::Thread{
public:
	ErrorPrinter();
	void errorCallback(const Error &error, bool cleared);
	void setEnabled(bool enable);
	bool getEnabled();
	void Run();

//	static ClassIdentifier info;
//	ClassIdentifier getInfo();
//
//	const ClassType getClassType(){return ClassType::Internal;};
private:
	bool enabled = true;
	//std::vector<Error> errorsToPrint;
};

#endif /* SRC_ERRORHANDLER_H_ */
