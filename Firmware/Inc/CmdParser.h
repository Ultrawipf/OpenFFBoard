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



enum class CMDtype{
	set,setat,get,getat,none,help,err
};
struct ParsedCommand
{
    std::string cmd;
    uint16_t adr = 0;
    int32_t val = 0;
    CMDtype type = CMDtype::none;

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

	const std::string helpstring = "Parser usage:\n Set <cmd>=<int>/<cmd>?<adr>=<var>\n Get: <cmd>?/<cmd>?<var>\nInfo: <cmd>!\ndelims: [;/CR/NL/SPACE]";
};

#endif /* CMDPARSER_H_ */
