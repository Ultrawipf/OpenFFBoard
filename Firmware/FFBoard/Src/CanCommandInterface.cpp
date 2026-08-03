/*
 * CanCommandInterface.cpp
 *
 *  Created on: 03.08.2026
 *      Author: Yannick
 */

#include "CanCommandInterface.h"
#ifdef CANBUS

#include "FFBoardMainCommandThread.h"
#include <cstring>

/***********************
 * CAN Command Interface
 * ******************
 */

CAN_CommandInterface::CAN_CommandInterface(uint32_t baseId,CANPort& port) :
	CommandInterface(),CanHandler(),cpp_freertos::Thread("CANCMD",256,18),
	port(port),baseId(baseId)
{
	port.takePort();

	CAN_filter f;
	f.active = true;
	f.extid = false;
	f.buffer = 0; // FIFO0
	f.filter_mask = 0x7FF; // exact match, standard 11bit id

	f.filter_id = baseId;
	filterIdHeader = port.addCanFilter(f);
	f.filter_id = baseId + 1;
	filterIdData = port.addCanFilter(f);
	f.filter_id = baseId + 2;
	filterIdAddr = port.addCanFilter(f);

	this->Start();
}

CAN_CommandInterface::~CAN_CommandInterface(){
	if(filterIdHeader >= 0){
		port.removeCanFilter(static_cast<uint8_t>(filterIdHeader));
	}
	if(filterIdData >= 0){
		port.removeCanFilter(static_cast<uint8_t>(filterIdData));
	}
	if(filterIdAddr >= 0){
		port.removeCanFilter(static_cast<uint8_t>(filterIdAddr));
	}
	port.freePort();
}

bool CAN_CommandInterface::needsVal(CanCmdType type){
	return type == CanCmdType::write || type == CanCmdType::writeAddr;
}

bool CAN_CommandInterface::needsAdr(CanCmdType type){
	return type == CanCmdType::writeAddr || type == CanCmdType::requestAddr;
}

void CAN_CommandInterface::resetAccum(){
	accum = {};
}

/**
 * Checks if the currently accumulating command has all required parts and,
 * if so, pushes it into the rx ring buffer and resets the accumulator.
 * Called only from canRxPendCallback (ISR context). Allocation free.
 */
void CAN_CommandInterface::dispatchAccum(){
	if(!accum.headerSeen){
		return;
	}
	if(needsVal(accum.type) && !accum.valSeen){
		return;
	}
	if(needsAdr(accum.type) && !accum.adrSeen){
		return;
	}

	size_t nextTail = (rxTail + 1) % RX_QUEUE_CAP;
	if(nextTail == rxHead){
		rxDroppedCount++; // queue full, drop
	}else{
		CanRawCommand& raw = rxQueue[rxTail];
		raw.type = accum.type;
		raw.clsid = accum.clsid;
		raw.instance = accum.instance;
		raw.cmdId = accum.cmdId;
		raw.val = accum.val;
		raw.adr = accum.adr;
		rxTail = nextTail;
		FFBoardMainCommandThread::wakeUp();
	}
	resetAccum();
}

/**
 * Runs inside the HAL CAN RX interrupt. Must not allocate or block.
 */
void CAN_CommandInterface::canRxPendCallback(CANPort* rxPort,CAN_rx_msg& msg){
	if(rxPort != &this->port || msg.header.rtr){
		return;
	}

	uint32_t id = msg.header.id;
	if(id == baseId){ // header frame
		if(msg.header.length < 8){
			return; // malformed
		}
		resetAccum();
		accum.headerSeen = true;
		accum.type = static_cast<CanCmdType>(msg.data[0]);
		uint16_t clsid; std::memcpy(&clsid,&msg.data[1],2); accum.clsid = clsid;
		accum.instance = msg.data[3];
		uint32_t cmdId; std::memcpy(&cmdId,&msg.data[4],4); accum.cmdId = cmdId;
		if(msg.header.length >= 16){ // combined frame (future CAN-FD)
			std::memcpy(&accum.val,&msg.data[8],8);
			accum.valSeen = true;
		}
		if(msg.header.length >= 24){ // combined frame (future CAN-FD)
			std::memcpy(&accum.adr,&msg.data[16],8);
			accum.adrSeen = true;
		}
		dispatchAccum();
	}else if(id == baseId + 1){ // data frame
		if(msg.header.length < 8){
			return;
		}
		if(!accum.headerSeen){
			rxDroppedCount++;
			return;
		}
		std::memcpy(&accum.val,msg.data,8);
		accum.valSeen = true;
		dispatchAccum();
	}else if(id == baseId + 2){ // addr frame
		if(msg.header.length < 8){
			return;
		}
		if(!accum.headerSeen){
			rxDroppedCount++;
			return;
		}
		std::memcpy(&accum.adr,msg.data,8);
		accum.adrSeen = true;
		dispatchAccum();
	}
	// any other id is not ours, ignore
}

bool CAN_CommandInterface::hasNewCommands(){
	return rxHead != rxTail;
}

/**
 * Drains the rx ring buffer and resolves targets. Task context only (called from FFBoardMainCommandThread).
 */
bool CAN_CommandInterface::getNewCommands(std::vector<ParsedCommand>& commands){
	bool any = false;
	while(rxHead != rxTail){
		CanRawCommand raw = rxQueue[rxHead];
		rxHead = (rxHead + 1) % RX_QUEUE_CAP;

		ParsedCommand cmd;
		cmd.cmdId = raw.cmdId;
		cmd.val = raw.val;
		cmd.adr = raw.adr;
		cmd.instance = raw.instance;

		switch(raw.type){
		case CanCmdType::write: cmd.type = CMDtype::set; break;
		case CanCmdType::request: cmd.type = CMDtype::get; break;
		case CanCmdType::writeAddr: cmd.type = CMDtype::setat; break;
		case CanCmdType::requestAddr: cmd.type = CMDtype::getat; break;
		}

		if(raw.instance != 0xFF){
			CommandHandler* target = CommandHandler::getHandlerFromId(raw.clsid,raw.instance);
			if(target == nullptr || !target->isValidCommandId(cmd.cmdId,CMDFLAG_STR_ONLY)){
				queueNotFound(raw.clsid,raw.instance,raw.cmdId);
				continue;
			}
			cmd.target = target;
			commands.push_back(cmd);
			any = true;
		}else{
			std::vector<CommandHandler*> handlers = CommandHandler::getHandlersFromId(raw.clsid);
			if(handlers.empty()){
				queueNotFound(raw.clsid,raw.instance,raw.cmdId);
				continue;
			}
			for(CommandHandler* handler : handlers){
				if(handler == nullptr || !handler->isValidCommandId(cmd.cmdId,CMDFLAG_STR_ONLY)){
					continue; // skip just this instance, not the whole broadcast
				}
				ParsedCommand newCmd = cmd;
				newCmd.target = handler;
				commands.push_back(newCmd);
				any = true;
			}
		}
	}
	return any;
}

bool CAN_CommandInterface::readyToSend(){
	return this->outBuffer.size() < maxQueuedReplies;
}

void CAN_CommandInterface::pushTxFrame(uint32_t id,const uint8_t* data,uint32_t len){
	if(this->outBuffer.size() >= maxQueuedReplies){
		return;
	}
	CAN_tx_msg msg;
	msg.header.id = id;
	msg.header.length = len;
	msg.header.extId = false;
	msg.header.rtr = false;
	std::memcpy(msg.data,data,len);
	this->outBuffer.push_back(msg);
}

void CAN_CommandInterface::queueReplyValues(const CommandReply& reply,const ParsedCommand& command){
	CmdHandlerInfo* info = command.target->getCommandHandlerInfo();
	uint16_t clsid = info->clsTypeid;
	uint8_t instance = info->instance;
	uint32_t cmdId = command.cmdId;

	CanReplyType type;
	bool sendVal = false;
	bool sendAdr = false;

	switch(reply.type){
	case CommandReplyType::ACK:
		type = CanReplyType::ack;
		break;
	case CommandReplyType::STRING_OR_DOUBLEINT:
	case CommandReplyType::DOUBLEINTS:
		type = CanReplyType::doubleInt;
		sendVal = true;
		sendAdr = true;
		break;
	case CommandReplyType::INT:
	case CommandReplyType::STRING_OR_INT:
		type = CanReplyType::int_;
		sendVal = true;
		break;
	case CommandReplyType::ERR:
		type = CanReplyType::err;
		break;
	case CommandReplyType::NONE:
	case CommandReplyType::STRING:
	default:
		return; // ignore, no numerical representation
	}

	uint8_t header[8];
	header[0] = static_cast<uint8_t>(type);
	std::memcpy(&header[1],&clsid,2);
	header[3] = instance;
	std::memcpy(&header[4],&cmdId,4);
	pushTxFrame(baseId + 3,header,8);

	if(sendVal){
		uint8_t data[8];
		std::memcpy(data,&reply.val,8);
		pushTxFrame(baseId + 4,data,8);
	}
	if(sendAdr){
		uint8_t adr[8];
		std::memcpy(adr,&reply.adr,8);
		pushTxFrame(baseId + 5,adr,8);
	}
}

void CAN_CommandInterface::queueNotFound(uint16_t clsid,uint8_t instance,uint32_t cmdId){
	uint8_t header[8];
	header[0] = static_cast<uint8_t>(CanReplyType::notFound);
	std::memcpy(&header[1],&clsid,2);
	header[3] = instance;
	std::memcpy(&header[4],&cmdId,4);
	pushTxFrame(baseId + 3,header,8);
	this->Notify();
}

/**
 * Called by FFBoardMainCommandThread once per executed command, for every registered interface.
 */
void CAN_CommandInterface::sendReplies(const std::vector<CommandResult>& results,CommandInterface* originalInterface){
	for(const CommandResult& result : results){
		if(result.type == CommandStatus::NO_REPLY){
			continue;
		}
		const std::vector<CommandReply>& replies = result.reply;
		if(originalInterface != this && !enableBroadcastFromOtherInterfaces){
			continue; // Skip
		}
		if( (result.type == CommandStatus::OK && replies.empty()) || (result.type == CommandStatus::ERR && replies.empty()) ){
			if(this->outBuffer.size() >= maxQueuedReplies){
				continue;
			}

			CommandReply reply;
			if(result.originalCommand.type == CMDtype::set){
				reply.type = CommandReplyType::INT;
				reply.val = result.originalCommand.val;
			}else if(result.originalCommand.type == CMDtype::setat){
				reply.type = CommandReplyType::DOUBLEINTS;
				reply.val = result.originalCommand.val;
				reply.adr = result.originalCommand.adr;
			}else if( (result.originalCommand.type == CMDtype::getat || result.originalCommand.type == CMDtype::get) && replies.empty()){
				reply.type = CommandReplyType::ACK;
				reply.adr = result.originalCommand.adr;
			}else if(result.type == CommandStatus::ERR){
				reply.type = CommandReplyType::ERR;
			}else{
				continue;
			}
			this->queueReplyValues(reply,result.originalCommand);
		}

		for(const CommandReply& reply : replies){
			if(reply.type == CommandReplyType::STRING){
				continue; // no string support on CAN
			}
			this->queueReplyValues(reply,result.originalCommand);
		}
	}
	if(!this->outBuffer.empty()){
		this->Notify();
	}
}

void CAN_CommandInterface::Run(){
	while(true){
		this->WaitForNotification();
		for(CAN_tx_msg& msg : outBuffer){
			port.sendMessage(msg);
		}
		outBuffer.clear();
		if(outBuffer.capacity() > 20){
			outBuffer.shrink_to_fit();
		}
	}
}

#endif
