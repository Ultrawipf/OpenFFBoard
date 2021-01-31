/*
 * cmdparser.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#include <CmdParser.h>


CmdParser::CmdParser() {

}

CmdParser::~CmdParser() {
}

void CmdParser::clear(){
	buffer.clear();
}


bool CmdParser::add(char* Buf, uint32_t *Len){
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



std::vector<ParsedCommand> CmdParser::parse(){

	std::vector<ParsedCommand> commands;
	std::vector<std::string> tokens;

    uint32_t pos = 0;
    uint32_t lpos = 0;
	while(pos < buffer.length()-1){
		pos = buffer.find(';',lpos);
		if(pos != std::string::npos){
			std::string token = buffer.substr(lpos,pos-lpos);
			lpos = pos+1;
			tokens.push_back(token);
		}
	}
	for(std::string word : tokens){

		ParsedCommand cmd;
		cmd.rawcmd = word;

		if(word.back() == '?'){ // <cmd>?
			cmd.type = CMDtype::get;
			cmd.cmd = word.substr(0, word.length()-1);

		}else if(word.back() == '!'){
			cmd.cmd = word.substr(0, word.length()-1);
			cmd.type = CMDtype::help;

		}else if(word.back() == '='){
			cmd.cmd = word;

			cmd.type = CMDtype::err;

		}else{
			uint32_t peq = word.find('=', 0); // set
			uint32_t pqm = word.find('?', 0); // read with var

			// <cmd>\n
			if(pqm == std::string::npos && peq == std::string::npos){
				cmd.cmd = word;
				cmd.type = CMDtype::get;

			}else{ // More complex

				// Check if conversion is even possible
				bool validPqm = (pqm != std::string::npos && (std::isdigit(word[pqm+1]) || (std::isdigit(word[pqm+2]) && (word[pqm+1] == '-' || word[pqm+1] == '+' || word[pqm+1] == 'x'))));
				bool validPeq = (peq != std::string::npos && (std::isdigit(word[peq+1]) || (std::isdigit(word[peq+2]) && (word[peq+1] == '-' || word[peq+1] == '+' || word[peq+1] == 'x'))));

				if(validPqm && validPeq && pqm < peq && (abs(peq - pqm) > 1)){ // <cmd>?<int>=<int>
					// Dual
					int32_t val;
					if(word[pqm+1] == 'x'){
						val = (int64_t)std::stoll(word.substr(pqm+2, peq-pqm),0,16);
					}else{
						val = (int64_t)std::stoll(word.substr(pqm+1, peq-pqm));
					}
					int64_t val2;
					if(word[peq+1] == 'x'){
						val2 = (int64_t)std::stoll(word.substr(peq+2, word.npos),0,16);
					}else{
						val2 = (int64_t)std::stoll(word.substr(peq+1, word.npos));
					}

					cmd.cmd = word.substr(0, pqm);
					cmd.type = CMDtype::setat;
					cmd.val = val2;
					cmd.adr = val;

				}else if(validPqm){ // <cmd>?<int>
					int64_t val;
					if(word[pqm+1] == 'x'){
						val = (int64_t)std::stoll(word.substr(pqm+1, word.npos),0,16);
					}else{
						val = (int64_t)std::stoll(word.substr(pqm+1, word.npos));
					}

					cmd.val = val;
					cmd.type = CMDtype::getat;
					cmd.cmd = word.substr(0, pqm);
					cmd.adr = val;

				}else if(validPeq){ // <cmd>=<int>
					int64_t val;
					if(word[peq+1] == 'x'){
						val = (int64_t)std::stoll(word.substr(peq+1, word.npos),0,16);
					}else{
						val = (int64_t)std::stoll(word.substr(peq+1, word.npos));
					}

					cmd.val = val;
					cmd.type = CMDtype::set;
					cmd.cmd = word.substr(0, peq);

				}else{
					continue;
				}
			}
		}

		commands.push_back(cmd);
	}

	buffer.erase(0,lpos); // Clear parsed portion from buffer
	return commands;
}
