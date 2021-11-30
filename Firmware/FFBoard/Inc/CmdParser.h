/*
 * cmdparser.h
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#ifndef CMDPARSER_H_
#define CMDPARSER_H_
//#include "main.h"
#include <string>
#include <cstring>
#include "vector"
#include "ErrorHandler.h"
//#include "CommandInterface.h"
#include <optional>
#include "CommandHandler.h"

class CommandHandler;
class CommandInterface;

class CmdParser {
public:
	CmdParser();
	virtual ~CmdParser();

	void clear();
	bool add(char* Buf, uint32_t *Len);
	bool parse(std::vector<ParsedCommand>& commands);

private:
	std::string buffer;
};

#endif /* CMDPARSER_H_ */
