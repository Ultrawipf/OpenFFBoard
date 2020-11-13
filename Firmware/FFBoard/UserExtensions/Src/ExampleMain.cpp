/*
 * exampleMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include <ExampleMain.h>

// Change this
ClassIdentifier ExampleMain::info = {
		 .name = "Custom" ,
		 .id=1337,
		 .hidden=true //Set false to list
 };
// Copy this to your class for identification
const ClassIdentifier ExampleMain::getInfo(){
	return info;
}



ExampleMain::ExampleMain() {

}

ExampleMain::~ExampleMain() {

}

ParseStatus ExampleMain::command(ParsedCommand* cmd,std::string* reply){
	ParseStatus flag = ParseStatus::OK; // Valid command found

	// ------------ commands ----------------
	if(cmd->cmd == "command"){ // Example: command=1337\n
		if(cmd->type == CMDtype::get){
			*reply+= std::to_string(examplevar);
		}else if(cmd->type == CMDtype::set){
			examplevar = cmd->val;
		}else{
			flag = ParseStatus::ERR;
		}
	}else{
		flag = ParseStatus::NOT_FOUND; // No valid command
	}
	return flag;
}
