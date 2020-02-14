/*
 * exampleMain.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include <ExampleMain.h>

// Change this
FFBoardMainIdentifier ExampleMain::info = {
		 .name = "Custom" ,
		 .id=1337,
		 .hidden=true //Set false to list in "lsconf"
 };
// Copy this to your class for identification
const FFBoardMainIdentifier ExampleMain::getInfo(){
	return info;
}



ExampleMain::ExampleMain() {
	// TODO Auto-generated constructor stub

}

ExampleMain::~ExampleMain() {
	// TODO Auto-generated destructor stub
}

bool ExampleMain::executeUserCommand(ParsedCommand* cmd,std::string* reply){
	bool flag = true; // Valid command found

	// ------------ commands ----------------
	if(cmd->cmd == "command"){ // Example: command=1337\n
		if(cmd->type == CMDtype::get){
			*reply+= std::to_string(examplevar);
		}else if(cmd->type == CMDtype::set){
			examplevar = cmd->val;
		}else{
			*reply += "Err";
		}
	}else{
		flag = false; // No valid command
	}
	return flag;
}
