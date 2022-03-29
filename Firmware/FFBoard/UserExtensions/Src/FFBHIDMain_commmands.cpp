/*
 * FFBWheel_commands.cpp
 *
 *  Created on: 28.01.2021
 *      Author: Yannick / Lidgard
 */


#include <FFBHIDMain.h>

void FFBHIDMain::registerCommands(){
	//CommandHandler::registerCommands();

	//registerCommand("axes", FFBWheel_commands::axes, "Number of axes (1-2)");
	registerCommand("ffbactive", FFBWheel_commands::ffbactive, "FFB status");

	registerCommand("btntypes", FFBWheel_commands::btntypes, "Enabled button sources");
	registerCommand("addbtn", FFBWheel_commands::addbtn, "Enable button source");
	registerCommand("lsbtn", FFBWheel_commands::lsbtn, "Get available button sources");

	registerCommand("aintypes", FFBWheel_commands::aintypes, "Enabled analog sources");
	registerCommand("lsain", FFBWheel_commands::lsain, "Get available analog sources");
	registerCommand("addain", FFBWheel_commands::addain, "Enable analog source");

	registerCommand("hidrate", FFBWheel_commands::hidrate, "Get estimated effect update speed");
	registerCommand("hidsendspd", FFBWheel_commands::hidsendspd, "Change HID gamepad update rate");
}

CommandStatus FFBHIDMain::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<FFBWheel_commands>(cmd.cmdId)){
	case FFBWheel_commands::ffbactive:
	{
		if(cmd.type != CMDtype::get)
			return CommandStatus::ERR;

		int8_t flag = this->getFfbActive() ? 1 : 0;
		if(this->control.emergency){
			flag = -1;
		}
		replies.push_back(CommandReply(flag));
		break;
	}
//	case FFBWheel_commands::axes:
//		if(cmd.type == CMDtype::get){
//			replies.push_back(CommandReply(this->axes_manager->getAxisCount()));
//		}else if(cmd.type == CMDtype::set){
//			this->axes_manager->setAxisCount(cmd.val);
//		}
//		break;
	case FFBWheel_commands::btntypes:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(btnsources));
		}else if(cmd.type == CMDtype::set){
			setBtnTypes(cmd.val);
		}
		break;
	case FFBWheel_commands::lsbtn:
		btn_chooser.replyAvailableClasses(replies);
		break;
	case FFBWheel_commands::addbtn:
		if(cmd.type == CMDtype::set){
			this->addBtnType(cmd.val);
		}
		break;
	case FFBWheel_commands::aintypes:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(ainsources));
		}else if(cmd.type == CMDtype::set){
			setAinTypes(cmd.val);
		}
		break;
	case FFBWheel_commands::lsain:
		analog_chooser.replyAvailableClasses(replies);
		break;
	case FFBWheel_commands::addain:
		if(cmd.type == CMDtype::set){
			this->addAinType(cmd.val);
		}
		break;
	case FFBWheel_commands::hidrate:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(this->getRate()));
		}else{
			return CommandStatus::ERR;
		}
		break;
	case FFBWheel_commands::hidsendspd:
		if(cmd.type == CMDtype::get){
			replies.push_back(CommandReply(usb_report_rate_idx));
		}else if(cmd.type == CMDtype::set){
			setReportRate(cmd.val);
		}else if(cmd.type == CMDtype::info){
			replies.push_back(CommandReply(usb_report_rates_names()));
		}
		break;
	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
};



