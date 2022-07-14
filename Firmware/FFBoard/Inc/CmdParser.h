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
#include "ringbufferwrapper.h"


class CommandHandler;
class CommandInterface;

class CmdParser {
public:
	CmdParser(uint32_t bufferMaxCapacity = 1024);
	virtual ~CmdParser();

	void clear();
	bool add(char* Buf, uint32_t *Len);
	bool parse(std::vector<ParsedCommand>& commands);
	int32_t bufferCapacity();

	void setClearBufferTimeout(uint32_t timeout);

private:
	//std::string buffer;
	//uint32_t reservedBuffer = 100;
	uint32_t bufferMaxCapacity = 512;

	uint32_t clearBufferTimeout = 0;
	uint32_t lastAddTime = 0;

	RingBufferWrapper ringbuffer;
};

#endif /* CMDPARSER_H_ */
