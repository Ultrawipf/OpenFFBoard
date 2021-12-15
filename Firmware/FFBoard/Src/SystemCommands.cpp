/*
 * SystemCommands.cpp
 *
 *  Created on: 30.11.2021
 *      Author: Yannick
 */

#include "SystemCommands.h"
#include "PersistentStorage.h"
#include "FFBoardMain.h"
#include "voltagesense.h"
#include <malloc.h>
#include "constants.h"
#include "task.h"
#include "FreeRTOSConfig.h"
extern ClassChooser<FFBoardMain> mainchooser;
extern FFBoardMain* mainclass;
//extern static const uint8_t SW_VERSION_INT[3];

bool SystemCommands::allowDebugCommands = false;

ClassIdentifier SystemCommands::info = {
		 .name = "System Commands" ,
		 .id = CLSID_SYSTEM,
		 .hidden = true
 };

const ClassIdentifier SystemCommands::getInfo(){
	return info;
}

SystemCommands::SystemCommands() : CommandHandler(CMDCLSTR_SYS, CMDCLSID_SYS) {
	registerCommands();
	//CommandHandler::registerCommands();

}

SystemCommands::~SystemCommands() {

}


void SystemCommands::registerCommands(){
	CommandHandler::registerCommand("help", FFBoardMain_commands::help, "Print system help",CMDFLAG_STR_ONLY);
	CommandHandler::registerCommand("save", FFBoardMain_commands::save, "Write all settings to flash",CMDFLAG_GET);
	CommandHandler::registerCommand("reboot", FFBoardMain_commands::reboot, "Reset chip",CMDFLAG_GET);
	CommandHandler::registerCommand("dfu", FFBoardMain_commands::dfu, "reboot into DFU bootloader",CMDFLAG_GET);
	CommandHandler::registerCommand("lsmain", FFBoardMain_commands::lsmain, "List available mainclasses",CMDFLAG_GET);
	CommandHandler::registerCommand("lsactive", FFBoardMain_commands::lsactive, "List active classes (Fullname:clsname:inst:clsid:idx)",CMDFLAG_GET);
	CommandHandler::registerCommand("vint", FFBoardMain_commands::vint, "Internal voltage(mV)",CMDFLAG_GET);
	CommandHandler::registerCommand("vext", FFBoardMain_commands::vext, "External voltage(mV)",CMDFLAG_GET);
	CommandHandler::registerCommand("main", FFBoardMain_commands::main, "Query or change mainclass",CMDFLAG_GET | CMDFLAG_SET);
	CommandHandler::registerCommand("swver", FFBoardMain_commands::swver, "Firmware version",CMDFLAG_GET);
	CommandHandler::registerCommand("hwtype", FFBoardMain_commands::hwtype, "Hardware type",CMDFLAG_GET);
	CommandHandler::registerCommand("flashraw", FFBoardMain_commands::flashraw, "Write value to flash address",CMDFLAG_SETADR);
	CommandHandler::registerCommand("flashdump", FFBoardMain_commands::flashdump, "Read all flash variables",CMDFLAG_GET);
	CommandHandler::registerCommand("errors", FFBoardMain_commands::errors, "Read error states",CMDFLAG_GET);
	CommandHandler::registerCommand("errorsclr", FFBoardMain_commands::errorsclr, "Reset errors",CMDFLAG_GET);
	CommandHandler::registerCommand("heapfree", FFBoardMain_commands::heapfree, "Memory info",CMDFLAG_GET);
#if configUSE_STATS_FORMATTING_FUNCTIONS> 0
	CommandHandler::registerCommand("taskstats", FFBoardMain_commands::taskstats, "Task stats",CMDFLAG_GET);
#endif
	CommandHandler::registerCommand("format", FFBoardMain_commands::format, "set format=1 to erase all stored values",CMDFLAG_SET);
	CommandHandler::registerCommand("debug", FFBoardMain_commands::debug, "Enable or disable debug commands",CMDFLAG_SET | CMDFLAG_GET);
}

CommandStatus SystemCommands::internalCommand(const ParsedCommand& cmd,std::vector<CommandReply>& replies,CommandInterface* interface){
	CommandStatus flag = CommandStatus::OK;

	switch((FFBoardMain_commands)cmd.cmdId)
	{
		case FFBoardMain_commands::help:
		{// help
			if(cmd.type == CMDtype::info){
				replies.push_back(CommandReply(this->getCsvHelpstring()));
			}else{
				std::string reply =	interface->getHelpstring() + "\nAvailable classes (use cls.0.help for more info):\n";
				//std::string reply = "";
				for(CommandHandler* handler : CommandHandler::cmdHandlers){
					CmdHandlerInfo* info = handler->getCommandHandlerInfo();
					reply += std::string(info->clsname) + "." + std::to_string(info->instance)+"\n";
				}
				reply +=  "\n"+this->getCommandsHelpstring();
				replies.push_back(CommandReply(reply));
			}

			break;
		}

		case FFBoardMain_commands::save:
			for(PersistentStorage* handler : PersistentStorage::flashHandlers){
				handler->saveFlash();
			}
			break;

		case FFBoardMain_commands::reboot:
			NVIC_SystemReset();
			break;

		case FFBoardMain_commands::dfu:
			RebootDFU();
			break;
		case FFBoardMain_commands::lsmain:
			mainchooser.replyAvailableClasses(replies);
			break;

		case FFBoardMain_commands::vint:
		{
			replies.push_back(CommandReply(getIntV()));
			break;
		}

		case FFBoardMain_commands::debug:
			return handleGetSet(cmd, replies, SystemCommands::allowDebugCommands);

		case FFBoardMain_commands::vext:
		{
			replies.push_back(CommandReply(getExtV()));
			break;
		}

		case FFBoardMain_commands::main:
		{
			if(cmd.type == CMDtype::get){
				uint16_t buf=mainclass->getInfo().id;
				Flash_Read(ADR_CURRENT_CONFIG, &buf);
				replies.push_back(CommandReply(buf));
			}else if(cmd.type == CMDtype::set){
				if(mainchooser.isValidClassId(cmd.val)){
					Flash_Write(ADR_CURRENT_CONFIG, (uint16_t)cmd.val);
					if(cmd.val != mainclass->getInfo().id){
						NVIC_SystemReset(); // Reboot
					}
				}
			}
			break;
		}
		case FFBoardMain_commands::errors:
		{
			replyErrors(replies);
			break;
		}
		case FFBoardMain_commands::flashdump:
			replyFlashDump(replies);
			break;

		case FFBoardMain_commands::flashraw:
		{
			if(cmd.type == CMDtype::setat){
				Flash_Write(cmd.adr, cmd.val);

			}else if(cmd.type == CMDtype::getat){
				CommandReply reply;
				reply.type = CommandReplyType::INT;
				uint16_t val;
				if(Flash_Read(cmd.adr,&val)){
					reply.val=val;
				}
				replies.push_back(reply);
			}
			break;
		}

		case FFBoardMain_commands::errorsclr:
			ErrorHandler::clearAll();
			break;

		case FFBoardMain_commands::swver:
		{
			CommandReply reply;
			reply.type = CommandReplyType::STRING_OR_INT;

			extern const uint8_t SW_VERSION_INT[3];
			reply.reply += std::to_string(SW_VERSION_INT[0]) + "." + std::to_string(SW_VERSION_INT[1]) + "." + std::to_string(SW_VERSION_INT[2]);

			reply.val = (SW_VERSION_INT[0]<<16) | (SW_VERSION_INT[1] << 8) | (SW_VERSION_INT[2]);
			replies.push_back(reply);
			break;
		}

		case FFBoardMain_commands::hwtype:
		{
			replies.push_back(CommandReply(HW_TYPE,HW_TYPE_INT));
			break;
		}

		case FFBoardMain_commands::mallinfo: // UNUSED since freertos
		{
			CommandReply reply;
			struct mallinfo info = mallinfo();
			reply.adr = info.uordblks;
			reply.val = info.uordblks;
			reply.reply +="Usage: ";
			reply.reply += std::to_string(info.uordblks);
			reply.reply +=" Size: ";
			reply.reply +=std::to_string(info.arena);
			reply.type = CommandReplyType::STRING_OR_DOUBLEINT;
			replies.push_back(reply);
			break;
		}

		case FFBoardMain_commands::heapfree:
		{
			replies.push_back(CommandReply(xPortGetFreeHeapSize(),xPortGetMinimumEverFreeHeapSize()));
			break;
		}
#if configUSE_STATS_FORMATTING_FUNCTIONS>0
		case FFBoardMain_commands::taskstats:
		{
			char repl[800];
			vTaskGetRunTimeStats(repl);
			replies.push_back(CommandReply("\n"+std::string(repl)));
			break;
		}
#endif
		case FFBoardMain_commands::lsactive:
		{
			for(CommandHandler* handler : CommandHandler::cmdHandlers){
				if(handler->hasCommands()){
					ClassIdentifier i = handler->getInfo();
					CmdHandlerInfo* hi = handler->getCommandHandlerInfo();
					CommandReply reply;
					reply.type = CommandReplyType::STRING_OR_DOUBLEINT;
					reply.adr = hi->instance;
					reply.val = hi->clsTypeid;
					reply.reply += std::string(i.name)+ ":" + hi->clsname + ":" + std::to_string(hi->instance) + ":" + std::to_string(i.id) + ":" + std::to_string(hi->commandHandlerID);
					replies.push_back(reply);
				}
			}
			break;
		}
		case FFBoardMain_commands::format:
			if(cmd.type == CMDtype::set && cmd.val==1){
				HAL_FLASH_Unlock();
				EE_Format();
				HAL_FLASH_Lock();
			}
		break;

		default:
			flag = CommandStatus::NOT_FOUND;
			break;
	}
	return flag;
}

/*
 * Prints a formatted flash dump to the reply string
 */
void SystemCommands::replyFlashDump(std::vector<CommandReply>& replies){
	std::vector<std::tuple<uint16_t,uint16_t>> result;
	Flash_Dump(&result);

	for(auto entry : result){
		CommandReply reply;
		uint16_t adr;
		uint16_t val;
		std::tie(adr,val) = entry;
		//reply.reply += std::to_string(adr) + ":" + std::to_string(val) + "\n";
		reply.adr = adr;
		reply.val = val;
		reply.type = CommandReplyType::DOUBLEINTS;
		replies.push_back(reply);
	}
}

/*
 * Prints a formatted list of error conditions
 */
void SystemCommands::replyErrors(std::vector<CommandReply>& replies){
	std::vector<Error>* errors = ErrorHandler::getErrors();
	if(errors->size() == 0){
		CommandReply reply;
		reply.reply += "None";
		reply.type = CommandReplyType::STRING_OR_INT;
		replies.push_back(reply);
		return;
	}

	for(Error error : *errors){
		CommandReply reply;
		reply.reply += error.toString() + "\n";
		reply.val = (uint32_t)error.code;
		reply.type = CommandReplyType::STRING_OR_INT;
		replies.push_back(reply);
	}

	ErrorHandler::clearTemp();
}
