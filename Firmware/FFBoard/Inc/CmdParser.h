/*
 * cmdparser.h
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#ifndef CMDPARSER_H_
#define CMDPARSER_H_
#include "main.h"
#include <string>
#include <cstring>
#include "vector"
#include "ErrorHandler.h"

enum class ParseStatus : uint8_t {NOT_FOUND,OK,ERR,OK_CONTINUE,NO_REPLY};
enum class CommmandReplyType : uint8_t {NOT_FOUND,ACK,INT,STRING,NO_REPLY,STRING_OR_INT,FLOAT};

enum class CMDtype{
	set,setat,get,getat,none,help,err
};
struct ParsedCommand
{
	std::string cls = ""; // classname
    std::string cmd = "";
    int64_t adr = 0;
    int64_t val = 0;
    char prefix = '\0';
    //std::string rawcmd;
    CMDtype type = CMDtype::none;

};

struct CommandReply
{
    std::string reply="";
    int64_t adr = 0;
    int64_t val = 0;
    //char prefix = '\0';
    std::string rawcmd = "";
    ParseStatus result = ParseStatus::NOT_FOUND;
    CommmandReplyType type = CommmandReplyType::NOT_FOUND;
};

struct CommandResult {
	CommandReply reply;
	ParsedCommand originalCommand;
};


template<class T> std::string cmdSetGet(ParsedCommand* cmd,T* val){
	if(cmd->type == CMDtype::set){
		val = cmd->val;
		return "";
	}else if(cmd->type == CMDtype::get){
		std::string ret = std::to_string(*val);
		return ret;
	}
	return "Err";
}


class CmdParser {
public:
	CmdParser();
	virtual ~CmdParser();

	void clear();

	std::string buffer;

	bool add(char* Buf, uint32_t *Len);
	std::vector<ParsedCommand> parse();

	std::string formatReply(CommandResult& reply);
	const std::string helpstring = "Parser usage:\n Set cmd=int/cmd?adr=var\n Get: cmd?/cmd?var\nInfo: cmd!\ndelims: ;/CR/NL/SPACE\n";
private:
	Error cmdNotFoundError = Error(ErrorCode::cmdNotFound,ErrorType::temporary,"Invalid command");
	Error cmdExecError = Error(ErrorCode::cmdExecutionError,ErrorType::temporary,"Error while executing command");

};

#endif /* CMDPARSER_H_ */
