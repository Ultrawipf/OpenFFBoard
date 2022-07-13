/*
 * cmdparser.cpp
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#include <CmdParser.h>
#include "ErrorHandler.h"
#include "CommandHandler.h"
#include "FFBoardMainCommandThread.h"
#include "critical.hpp"

CmdParser::CmdParser(uint32_t reservedBuffer,uint32_t bufferMaxCapacity) {
	this->reservedBuffer = reservedBuffer;
	this->bufferMaxCapacity = bufferMaxCapacity;
	buffer.reserve(reservedBuffer);
}

CmdParser::~CmdParser() {
}

void CmdParser::clear(){
	buffer.clear();
}


// TODO: when called from interrupts quickly it might interfere with the parsing. Use double buffers or create tokens in this function
bool CmdParser::add(char* Buf, uint32_t *Len){
	if(bufferMaxCapacity < this->buffer.size()+*Len){
		//buffer.clear();
		pulseErrLed();
		return false; // we can not add to the buffer. reject command and clear buffer
	}

	bool flag = false;
	for(uint32_t i=0;i<*Len;i++){
		// Replace end markers
		if(*(Buf+i) == '\n' || *(Buf+i) == '\r' || *(Buf+i) == ';'|| *(Buf+i) == ' '){
			*(Buf+i) = (uint8_t)';';
			flag = true;

		}
	}
	if(clearBufferTimeout && HAL_GetTick() - lastAddTime > clearBufferTimeout ){
		this->buffer.clear();
	}
	lastAddTime = HAL_GetTick();
	this->buffer.append((char*)Buf,*Len);
	return flag;
}

/**
 * If the last command was earlier than the timeout force clear the buffer before appending.
 * Gets rid of unparsable parts in the buffer.
 * Set 0 to never force clear the buffer automatically and only clear parsed portions (default).
 */
void CmdParser::setClearBufferTimeout(uint32_t timeout){
	this->clearBufferTimeout = timeout;
}

int32_t CmdParser::bufferCapacity(){
	if(lastAddTime > clearBufferTimeout){
		return bufferMaxCapacity;
	}
	return std::max<int32_t>(bufferMaxCapacity - buffer.size(),0);
}


// Format: cls.instance.cmd<=|?|!><val?>
bool CmdParser::parse(std::vector<ParsedCommand>& commands){

	bool found = false;
	//std::vector<ParsedCommand> commands;
	std::vector<std::string> tokens;

	uint32_t pos = 0;
	uint32_t lpos = 0;

	cpp_freertos::CriticalSection::Enter();
	while(pos < buffer.length()-1){
		pos = buffer.find(';',lpos);
		if(pos != std::string::npos){
			std::string token = buffer.substr(lpos,pos-lpos);
			lpos = pos+1;
			tokens.push_back(token);
		}
	}
	buffer.erase(0,lpos); // Clear parsed portion from buffer
	buffer.reserve(reservedBuffer);
	cpp_freertos::CriticalSection::Exit();

	for(std::string &word : tokens){
		if(word.length() < 2)
			continue;

		ParsedCommand cmd;
		//cmd.rawcmd = word;
		uint8_t cmd_start = 0;

		uint32_t point1 = word.find('.', 0);
		uint32_t point2 = word.find('.', point1+1); // if has unique instance char


		// cmdstart = <cls>.
		std::string clsname;
		if(point1 != std::string::npos){
			cmd_start = point1+1;
			clsname = word.substr(0, point1);
			// Get the class id from the command handler list
			//cmd.classId = CommandHandler::getClassIdFromName(word.substr(0, point1));
		}
		// cmdstart = <cls>.x.
		if(point2 != std::string::npos){
			//cmd.prefix = word[point1+1];
			cmd.instance = word[point1+1] >= '0' ? word[point1+1] - '0' : 0;
			cmd_start = point2+1; // after second point
		}

		std::string cmdstring;
		if(word.back() == '?'){ // <cmd>?
			cmd.type = CMDtype::get;
			cmdstring = word.substr(cmd_start, word.length()-cmd_start - 1);
	
		}else if(word.back() == '!'){
			cmdstring = word.substr(cmd_start, word.length()-cmd_start - 1);
			cmd.type = CMDtype::info;
		
		}else if(word.back() == '='){
			cmdstring = word.substr(cmd_start, word.length()-cmd_start);

			cmd.type = CMDtype::err;

		}else{
			uint32_t peq = word.find('=', 0); // set
			uint32_t pqm = word.find('?', 0); // read with var

			// <cmd>\n
			if(pqm == std::string::npos && peq == std::string::npos){
				cmdstring = word.substr(cmd_start, word.length()-cmd_start);
				cmd.type = CMDtype::get;

			}else{ // More complex

				// Check if conversion is even possible
				bool validPqm = (pqm != std::string::npos && (std::isdigit(word[pqm+1]) || (std::isdigit(word[pqm+2]) && (word[pqm+1] == '-' || word[pqm+1] == '+')) || ( std::isxdigit(word[pqm+2]) && word[pqm+1] == 'x')));
				bool validPeq = (peq != std::string::npos && (std::isdigit(word[peq+1]) || (std::isdigit(word[peq+2]) && (word[peq+1] == '-' || word[peq+1] == '+')) || ( std::isxdigit(word[peq+2]) && word[peq+1] == 'x')));

				if(validPqm && validPeq && peq < pqm && (abs(pqm - peq) > 1)){ // <cmd>=<int>?<int>
					// Dual
					int64_t val;
					int64_t val2;
					if(word[pqm+1] == 'x'){
						val2 = (int64_t)std::stoll(&word[pqm+2],0,16);
					}else{
						val2 = (int64_t)std::stoll(&word[pqm+1]);
					}

					if(word[peq+1] == 'x'){
						val = (int64_t)std::stoll(word.substr(peq+2, pqm-peq),0,16);
					}else{
						val = (int64_t)std::stoll(word.substr(peq+1, pqm-peq));
					}

					cmdstring = word.substr(cmd_start, peq-cmd_start);
					cmd.type = CMDtype::setat;
					cmd.val = val;
					cmd.adr = val2;
				
				}else if(validPqm){ // <cmd>?<int>
					int64_t val;
					if(word[pqm+1] == 'x'){
						val = (int64_t)std::stoll(&word[pqm+2],0,16);
					}else{
						val = (int64_t)std::stoll(&word[pqm+1]);
					}

					cmd.val = val;
					cmd.type = CMDtype::getat;
					cmdstring = word.substr(cmd_start, pqm-cmd_start);
					cmd.adr = val;

				}else if(validPeq){ // <cmd>=<int>
					int64_t val;
					if(word[peq+1] == 'x'){
						val = (int64_t)std::stoll(&word[peq+2],0,16);
					}else{
						val = (int64_t)std::stoll(&word[peq+1]);
					}

					cmd.val = val;
					cmd.type = CMDtype::set;
					cmdstring = word.substr(cmd_start, peq-cmd_start);
				
				}else{
					continue;
				}
			}
		}


		if(clsname.empty()){
			clsname = "sys"; // No name passed. fallback to system commands
		}

		if(cmd.instance != 0xFF){
			cmd.target = (CommandHandler::getHandlerFromClassName(clsname.c_str(),cmd.instance));
			if(cmd.target == nullptr){
				continue; // invalid class
			}
			CmdHandlerCommanddef* cmdDef = cmd.target->getCommandFromName(cmdstring,CMDFLAG_HID_ONLY);

			if(cmdDef){
				cmd.cmdId = cmdDef->cmdId;
				commands.push_back(cmd);
				found = true;
			}


		}else{
			// Targeting all classes with this name. Need to get the command id from all of them
			std::vector<CommandHandler*> targets = CommandHandler::getHandlersFromClassName(clsname.c_str());

			for(CommandHandler* target : targets){
				CmdHandlerInfo* cmdhandlerinfo = target->getCommandHandlerInfo();
				ParsedCommand newCmd = cmd;
				newCmd.target = target;

				if(targets.size() > 1) // Get unique instance id if multiple results
					newCmd.instance = cmdhandlerinfo->instance;

				CmdHandlerCommanddef* cmdDef = newCmd.target->getCommandFromName(cmdstring,CMDFLAG_HID_ONLY);
				if(cmdDef){
					newCmd.cmdId = cmdDef->cmdId;
					commands.push_back(newCmd);
					found = true;
				}
			}
		}
		if(!found){
			Error error = FFBoardMainCommandThread::cmdNotFoundError;
			error.info += ":"+cmdstring;
			ErrorHandler::addError(error);
		}
	}

	return found;
}
