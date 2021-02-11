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


/*
 * Error code definitions
 */

enum class ErrorCode : uint32_t{
			none = 0,
			shutdown = 1,

			cmdNotFound = 5,
			cmdExecutionError = 6,

			undervoltage = 10,
			overvoltage = 11,

			overtemp = 15
};
/*
 * Error type for severity
 * temporary errors are cleared when read back
 * critical errors cause a shutdown or have to be cleared externally
 */
enum class ErrorType : uint8_t{
	warning,critical,temporary
};

struct Error_t{
	ErrorCode code = ErrorCode::none;
	ErrorType type = ErrorType::warning;
	std::string info = "";

	/*
	 * Prints the error to a string
	 */
	std::string toString(){
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
};




class ErrorHandler {
public:
	static std::vector<ErrorHandler*> errorHandlers;

	ErrorHandler();
	virtual ~ErrorHandler();
	virtual void errorCallback(Error_t &error, bool cleared); // Called when a new error happened

	static void addError(Error_t &error);
	static void clearError(Error_t &error);
	static void clearError(ErrorCode errorcode);
	static void clearTemp();
	static void clearAll();

	static std::vector<Error_t>* getErrors(); // Returns a vector of active errors

protected:
	static std::vector<Error_t> errors;
};

/*
 * Can handle errors occuring in the main class
 */
class ErrorPrinter : ErrorHandler{
public:
	void errorCallback(Error_t &error, bool cleared);
	void setEnabled(bool enable);
	bool getEnabled();
private:
	bool enabled = true;
};

#endif /* SRC_ERRORHANDLER_H_ */
