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

