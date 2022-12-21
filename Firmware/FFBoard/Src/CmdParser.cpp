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

/**
 * Constructs a string command parser
 * @param[in] bufferMaxCapacity capacity of the ringbuffer. Must be divisible by 2 and smaller than CMDPARSER_MAX_VALID_CAPACITY
 */
CmdParser::CmdParser(const uint32_t bufferMaxCapacity) :
	bufferMaxCapacity(std::min<size_t>(CMDPARSER_MAX_VALID_CAPACITY,bufferMaxCapacity)),
	ringbuffer(RingBufferWrapper(std::min<size_t>(CMDPARSER_MAX_VALID_CAPACITY,bufferMaxCapacity)))
{
	ringbuffer.clean();
}

CmdParser::~CmdParser() {
}

void CmdParser::clear(){
	ringbuffer.clean();
}


/**
 * Adds one or more characters to the parser buffer
 * Returns true if an end marker was detected and a command is ready to be parsed
 */
bool CmdParser::add(char* Buf, uint32_t *Len){

	if(clearBufferTimeout && HAL_GetTick() - lastAddTime > clearBufferTimeout ){
		clear();
	}

	bool flag = false;
	for(uint32_t i=0;i<*Len;i++){
		// Replace end markers
		char c = *(Buf+i);
		if(c == '\n' || c == '\r' || c == ';'|| c == ' '){
			*(Buf+i) = (uint8_t)';';
			flag = true;
		}
	}

	if(ringbuffer.freeSpace() > *Len){
		ringbuffer.appendMultiple((uint8_t*)Buf, *Len);
		lastAddTime = HAL_GetTick();
	}else{
		if(flag){
			flag = false;
			clear();
		}
	}

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

	return std::max<int32_t>(ringbuffer.freeSpace(),0);
}


// Format: cls.instance.cmd<=|?|!><val?>
bool CmdParser::parse(std::vector<ParsedCommand>& commands){

	bool found = false;

	uint32_t bufferlen = std::min<size_t>(CMDPARSER_MAX_VALID_CAPACITY,ringbuffer.length()); // Fixed bound for array
	if(bufferlen==0 || bufferlen > bufferMaxCapacity){
		return false;
	}
	char buf[bufferlen];
	ringbuffer.peekMultiple((uint8_t*)buf, bufferlen); // Copy new portion out of rinbuffer

	uint32_t pos = 0;
	uint32_t lpos = 0;

	while(pos<bufferlen){
		if(buf[pos] == ';'){ // find end marker
			if(lpos+2<pos){

				//tokens.emplace_back(buf+lpos,buf+pos);
				std::string word(buf+lpos,buf+pos);
				// Begin parsing

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
								val2 = (int64_t)std::strtoll(&word[pqm+2],0,16);
							}else{
								val2 = (int64_t)std::strtoll(&word[pqm+1],0,10);
							}

							if(word[peq+1] == 'x'){
								val = (int64_t)std::strtoll(word.substr(peq+2, pqm-peq).c_str(),0,16);
							}else{
								val = (int64_t)std::strtoll(word.substr(peq+1, pqm-peq).c_str(),0,10);
							}

							cmdstring = word.substr(cmd_start, peq-cmd_start);
							cmd.type = CMDtype::setat;
							cmd.val = val;
							cmd.adr = val2;

						}else if(validPqm){ // <cmd>?<int>
							int64_t val;
							if(word[pqm+1] == 'x'){
								val = (int64_t)std::strtoll(&word[pqm+2],0,16);
							}else{
								val = (int64_t)std::strtoll(&word[pqm+1],0,10);
							}

							cmd.val = val;
							cmd.type = CMDtype::getat;
							cmdstring = word.substr(cmd_start, pqm-cmd_start);
							cmd.adr = val;

						}else if(validPeq){ // <cmd>=<int>
							int64_t val;
							if(word[peq+1] == 'x'){
								val = (int64_t)std::strtoll(&word[peq+2],0,16);
							}else{
								val = (int64_t)std::strtoll(&word[peq+1],0,10);
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
					if(cmd.target != nullptr){
						CmdHandlerCommanddef* cmdDef = cmd.target->getCommandFromName(cmdstring,CMDFLAG_HID_ONLY);

						if(cmdDef){
							cmd.cmdId = cmdDef->cmdId;
							commands.push_back(cmd);
							found = true;
						}
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
					error.info += ":"+clsname+"."+cmdstring;
					ErrorHandler::addError(error);
				}
			}
			// End parsing
			lpos = pos+1; // Advance to last valid position after the ;
		}
		pos++;
	}


	// discard used chars from ringbuffer until the last found ;
	ringbuffer.discardMultiple(lpos);

	return found;
}
