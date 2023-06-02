/*
 * HidCommandInterface.cpp
 *
 *  Created on: 10.12.2021
 *      Author: Yannick
 */


#include "HidCommandInterface.h"
#include "CommandHandler.h"
#include "critical.hpp"

/***********************
 * HID Command Interface
 *
 * ******************
 */
//cpp_freertos::BinarySemaphore HID_CommandInterface::threadSem = cpp_freertos::BinarySemaphore();


HID_CommandInterface* HID_CommandInterface::globalInterface = nullptr;
HID_CommandInterface::HID_CommandInterface() : cpp_freertos::Thread("HIDCMD",128,18){
	globalInterface = this;
	this->Start();
}

HID_CommandInterface::~HID_CommandInterface(){
	globalInterface = nullptr;
}

bool HID_CommandInterface::getNewCommands(std::vector<ParsedCommand>& commands){
	commands = std::move(this->commands);
	this->commands.clear();
	parserReady = false;
	return !commands.empty();
}

// TODO maybe simplify and get rid of thread again if reliable
void HID_CommandInterface::Run(){
	while(true){
		this->WaitForNotification();
		for(HID_CMD_Data_t& rep : outBuffer){
			while(!tud_hid_n_ready(0)){
				Delay(1);
			}
			this->sendHidCmd(&rep);
		}
		outBuffer.clear();
		if(outBuffer.capacity() > 20){
			outBuffer.shrink_to_fit();
		}
	}
}

/**
 * true if output buffer contains data
 */
bool HID_CommandInterface::waitingToSend(){
	return !this->outBuffer.empty();
}

bool HID_CommandInterface::readyToSend(){
	return this->outBuffer.size() < maxQueuedReplies;
}

void HID_CommandInterface::sendReplies(const std::vector<CommandResult>& results,CommandInterface* originalInterface){

	for(const CommandResult& result : results){
		const std::vector<CommandReply>& replies = result.reply;
		if(result.type == CommandStatus::NO_REPLY) // Normally not possible at this point.
			continue;

		if( (originalInterface != this && enableBroadcastFromOtherInterfaces) || ( result.type == CommandStatus::OK && replies.empty() ) || (result.type == CommandStatus::ERR && replies.empty()) ){ // Request was sent by a different interface
			if(this->outBuffer.size() > maxQueuedRepliesBroadcast){
				continue; // for now we just throw away broadcasts if the buffer contains too many pending replies.
			}

			CommandReply reply; // Fake reply

			// return original command as get value if it was a set command
			if(result.originalCommand.type == CMDtype::set ){
				reply.type = CommandReplyType::INT;
				reply.val = result.originalCommand.val;
			}else if(result.originalCommand.type == CMDtype::setat){
				reply.type = CommandReplyType::DOUBLEINTS;
				reply.val = result.originalCommand.val;
				reply.adr = result.originalCommand.adr;
			}else if( (result.originalCommand.type == CMDtype::getat || result.originalCommand.type == CMDtype::get) && replies.empty()){
				// Send an ACK if there is no actual reply
				reply.type = CommandReplyType::ACK;
				reply.adr =  result.originalCommand.adr;
			}else if(result.type == CommandStatus::ERR){
				reply.type = CommandReplyType::ERR;
			}else{
				continue;
			}
			this->queueReplyValues(reply,result.originalCommand);

		}


		for(const CommandReply reply : replies){
			if(reply.type == CommandReplyType::STRING){
				continue; // Ignore string only replies
			}

			this->queueReplyValues(reply,result.originalCommand);
		}
	}
	if(!this->outBuffer.empty())
		this->Notify();
}


void HID_CommandInterface::queueReplyValues(const CommandReply& reply,const ParsedCommand& command){
	HID_CMD_Data_t hidReply;
	CmdHandlerInfo* info = command.target->getCommandHandlerInfo();
	hidReply.addr = reply.adr;
	hidReply.clsid = info->clsTypeid;
	hidReply.cmd = command.cmdId;
	hidReply.data = reply.val;
	hidReply.instance = info->instance;

	switch(reply.type){
	case CommandReplyType::ACK:
		// Echo
		hidReply.type = HidCmdType::ACK;
		break;
	case CommandReplyType::STRING_OR_DOUBLEINT:
	case CommandReplyType::DOUBLEINTS:
		// Return 2 ints
		hidReply.type = HidCmdType::requestAddr;
		break;
	case CommandReplyType::INT:
	case CommandReplyType::STRING_OR_INT:
		// Return 1 int
		hidReply.type = HidCmdType::request;
		break;
	case CommandReplyType::ERR:
		hidReply.type = HidCmdType::err;
		break;
	case CommandReplyType::NONE:
	case CommandReplyType::STRING:
	default:
		// Ignore
		return;

	}
	this->outBuffer.push_back(hidReply);
}

void HID_CommandInterface::transferComplete(uint8_t itf, uint8_t const* report, uint8_t len){

}



void HID_CommandInterface::hidCmdCallback(HID_CMD_Data_t* data){
	ParsedCommand cmd;
	cmd.cmdId = data->cmd;
	cmd.instance = data->instance;
	cmd.val = data->data;
	cmd.adr = data->addr;

	// Translate type
	if(data->type == HidCmdType::write){
		cmd.type = CMDtype::set;
	}else if(data->type == HidCmdType::request){
		cmd.type = CMDtype::get;
	}else if(data->type == HidCmdType::info){
		cmd.type = CMDtype::info;
	}else if(data->type == HidCmdType::writeAddr){
		cmd.type = CMDtype::setat;
	}else if(data->type == HidCmdType::requestAddr){
		cmd.type = CMDtype::getat;
	}



	if(data->instance != 0xff){
		cmd.target = CommandHandler::getHandlerFromId(data->clsid,data->instance);
		if(cmd.target == nullptr || !(cmd.target->isValidCommandId(cmd.cmdId, CMDFLAG_STR_ONLY))){
			data->type = HidCmdType::notFound;
			//sendHidCmd(data); // Send back error
			this->outBuffer.push_back(*data);
			this->Notify();
			return;
		}
		if(cmd.target->isValidCommandId(cmd.cmdId, CMDFLAG_STR_ONLY))
			commands.push_back(cmd);
	}else{
		std::vector<CommandHandler*> handlers = CommandHandler::getHandlersFromId(data->clsid);
		for(CommandHandler* handler : handlers){
			ParsedCommand newCmd = cmd;
			newCmd.target = handler;
			if(newCmd.target == nullptr || !(cmd.target->isValidCommandId(cmd.cmdId, CMDFLAG_STR_ONLY))){
				data->type = HidCmdType::notFound;
				//sendHidCmd(data); // Send back error
				this->outBuffer.push_back(*data);
				this->Notify();
				return;
			}
			commands.push_back(newCmd);
		}
	}

	if(!commands.empty()){
		parserReady = true; // Signals that we should execute commands in the thread
		FFBoardMainCommandThread::wakeUp();
	}
}

/*
 * Send a custom transfer with the vendor defined IN report
 */
bool HID_CommandInterface::sendHidCmd(HID_CMD_Data_t* data){
	bool res = false;
	res = tud_hid_n_report(0,0, reinterpret_cast<uint8_t*>(data), sizeof(HID_CMD_Data_t)); // sizeof(HID_CMD_Data_t)


	return res; // fail
}
