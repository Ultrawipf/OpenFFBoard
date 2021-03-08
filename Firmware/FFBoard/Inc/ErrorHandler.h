/*
 * ErrorHandler.h
 *
 *  Created on: 09.02.2021
 *      Author: Yannick
 */

#ifndef SRC_ERRORHANDLER_H_
#define SRC_ERRORHANDLER_H_
#include <vector>
#include <string>
#include "thread.hpp"

/*
 * Error code definitions
 */

enum class ErrorCode : uint32_t{
			none = 0,
			shutdown = 1,
			emergencyStop = 2,

			cmdNotFound = 5,
			cmdExecutionError = 6,

			undervoltage = 10,
			overvoltage = 11,

			overtemp = 15,

			encoderAlignmentFailed = 20,
			adcCalibrationError = 21
};
/*
 * Error type for severity
 * temporary errors are cleared when read back
 * critical errors cause a shutdown or have to be cleared externally
 */
enum class ErrorType : uint8_t{
	warning,critical,temporary
};

class Error{
public:
	Error();
	Error(ErrorCode code, ErrorType type, std::string info) : code(code), type(type), info(info){};
	ErrorCode code = ErrorCode::none;
	ErrorType type = ErrorType::warning;
	std::string info = "";

	std::string toString();

	/*
	 * Errors are equivalent if their code and type match
	 */
	bool operator ==(const Error &b){return (this->code == b.code && this->type == b.type && this->info == b.info);}

};





class ErrorHandler {
public:
	static std::vector<ErrorHandler*> errorHandlers;

	ErrorHandler();
	virtual ~ErrorHandler();
	virtual void errorCallback(Error &error, bool cleared); // Called when a new error happened. Warning: Could be called from ISR!

	static void addError(Error error);
	static void clearError(Error error);
	static void clearError(ErrorCode errorcode);
	static void clearTemp();
	static void clearAll();

	static std::vector<Error>* getErrors(); // Returns a vector of active errors

protected:
	static std::vector<Error> errors;
};

/*
 * Can handle errors occuring in the main class
 */
class ErrorPrinter : ErrorHandler, cpp_freertos::Thread{
public:
	ErrorPrinter();
	void errorCallback(Error &error, bool cleared);
	void setEnabled(bool enable);
	bool getEnabled();
	void Run();
private:
	bool enabled = true;
	std::vector<Error> errorsToPrint;
};

#endif /* SRC_ERRORHANDLER_H_ */
