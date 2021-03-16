/*
 * FFBWheel_commands.cpp
 *
 *  Created on: 28.01.2021
 *      Author: Yannick / Lidgard
 */


#include "FFBWheel.h"


ParseStatus FFBWheel::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus flag = ParseStatus::OK;
	// ------------ General commands ----------------
	if(cmd->cmd == "axis"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->axes_manager->getAxisCount());
		}else if(cmd->type == CMDtype::set){
			if (this->axes_manager->setAxisCount(cmd->val)) {
				//Give the new axis effects handlers the effects array address
			}else {
				flag = ParseStatus::ERR;
				*reply += "Invalid no' of axis - Range 1-3 (X-Z)";
			}
		}
	}else if(cmd->cmd == "btntypes"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->btnsources);
		}else if(cmd->type == CMDtype::set){
			setBtnTypes(cmd->val);
		}else{
			flag = ParseStatus::ERR;
			*reply += "bin flag encoded list of button sources. (3 = source 1 & 2 active). See lsbtn for sources";
		}
	}else if(cmd->cmd == "addbtn"){
		if(cmd->type == CMDtype::set){
			this->addBtnType(cmd->val);
		}

	}else if(cmd->cmd == "lsbtn"){
		if(cmd->type == CMDtype::get){
			*reply += btn_chooser.printAvailableClasses();
		}
	}else if(cmd->cmd == "aintypes"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->ainsources);
		}else if(cmd->type == CMDtype::set){
			setAinTypes(cmd->val);
		}else{
			flag = ParseStatus::ERR;
			*reply += "bin flag encoded list of analog sources. (3 = source 1 & 2 active). See lsain for sources";
		}
	}else if(cmd->cmd == "lsain"){
		if(cmd->type == CMDtype::get){
			*reply += analog_chooser.printAvailableClasses();
		}
	}else if(cmd->cmd == "addain"){
		if(cmd->type == CMDtype::set){
			this->addAinType(cmd->val);
		}
	}else if(cmd->cmd == "hidrate" && cmd->type == CMDtype::get){
		*reply += std::to_string(this->getRate());
	}else if(cmd->cmd == "ffbactive" && cmd->type == CMDtype::get){
		if(this->control.emergency){
			*reply += "-1"; // Emergency
		}else{
			*reply += this->getFfbActive() ? "1" : "0";
		}
	}else{
		flag = ParseStatus::NOT_FOUND;
	}

	return flag;
};



