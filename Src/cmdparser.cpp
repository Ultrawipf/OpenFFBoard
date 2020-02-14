/*
 * cmdparser.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#include "cmdparser.h"


cmdparser::cmdparser() {
	// TODO Auto-generated constructor stub

}

cmdparser::~cmdparser() {
	// TODO Auto-generated destructor stub
}

void cmdparser::clear(){
	buffer.clear();
}


bool cmdparser::add(char* Buf, uint32_t *Len){
	bool flag = false;
	for(uint32_t i=0;i<*Len;i++){
		// Replace end markers
		if(*(Buf+i) == '\n' || *(Buf+i) == '\r' || *(Buf+i) == ';'|| *(Buf+i) == ' '){
			*(Buf+i) = (uint8_t)';';
			flag = true;
		}
	}

	this->buffer.append((char*)Buf,*Len);

	return flag;
}

//void execGetSet(ParsedCommand cmd,std::string* reply,){
// TODO?
//}

std::vector<ParsedCommand> cmdparser::parse(){

	std::vector<ParsedCommand> commands;
	std::vector<std::string> tokens;

    uint16_t pos = 0;
    uint16_t lpos = 0;
	while(pos < buffer.length()-1){
		pos = buffer.find(';',lpos);
		std::string token = buffer.substr(lpos,pos-lpos);
		lpos = pos+1;
		tokens.push_back(token);
	}

	for(std::string word : tokens){

		ParsedCommand cmd;

		if(word.back() == '?'){ // <cmd>?
			cmd.type = CMDtype::get;
			cmd.cmd = word.substr(0, word.length()-1);
		}else if(word.back() == '!'){
			cmd.cmd = word.length()-1;
			cmd.type = CMDtype::help;

		}else if(word.back() == '='){
			cmd.cmd = word;
			cmd.type = CMDtype::err;

		}else{ // More complex
			uint32_t peq = word.find('=', 0); // set
			//uint32_t pex = word.find('!', 0); // dual val
			uint32_t pqm = word.find('?', 0); // read with var
			if(pqm!=std::string::npos && pqm < peq && peq != std::string::npos){ // <cmd>?<int>=<int>
				// Dual
				int32_t val = (int32_t)std::stol(word.substr(pqm+1, peq-pqm));
				int32_t val2 = (int32_t)std::stol(word.substr(peq+1, word.npos));
				cmd.cmd = word.substr(0, pqm);
				cmd.type = CMDtype::setat;
				cmd.val = val2;
				cmd.adr = val;

			}else if(pqm != std::string::npos && (std::isdigit(word[pqm+1]) || (std::isdigit(word[pqm+2]) && (word[pqm+1] == '-' || word[pqm+1] == '+')))){ // <cmd>?<int>
				int32_t val = (int32_t)std::stol(word.substr(pqm+1, word.npos));
				cmd.val = val;
				cmd.type = CMDtype::getat;
				cmd.cmd = word.substr(0, pqm);

			}else if(peq != std::string::npos && (std::isdigit(word[peq+1]) || (std::isdigit(word[peq+2]) && (word[peq+1] == '-' || word[peq+1] == '+')))){ // <cmd>=<int>
				int32_t val = (int32_t)std::stol(word.substr(peq+1, word.npos));
				cmd.val = val;
				cmd.type = CMDtype::set;
				cmd.cmd = word.substr(0, peq);

			}else{
				cmd.cmd = word;
				cmd.type = CMDtype::get;
			}

		}

		commands.push_back(cmd);
	}
	buffer.clear();
	return commands;
}
