/*
 * mainclass_chooser.cpp
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */
#include "vector"
#include "cppmain.h"
#include "mainclass_chooser.h"

#include "ExampleMain.h"
#include "FFBWheel.h"
#include "FFBoardMain.h"
#include "TMCDebugBridge.h"


// Add all classes here
std::vector<class_entry> class_registry =
{
		add_class<FFBoardMain>(),
		add_class<FFBWheel>(),
		add_class<ExampleMain>(),
		add_class<TMCDebugBridge>()

};


std::string printAvailableClasses(){
	std::string ret;
	for(class_entry cls : class_registry){
		if(cls.info.hidden){
			continue;
		}
		ret+= std::to_string(cls.info.id);
		ret+= ":";
		ret+= cls.info.name;
		ret+='\n';
	}
	return ret;
}

//std::vector<uint16_t> getClassIds(){
//	std::vector<uint16_t> ids;
//	for(class_entry cls : class_registry){
//		ids.push_back(cls.info.id);
//	}
//	return ids;
//}

bool isValidClassId(uint16_t id){
	for(class_entry cls : class_registry){
		if(cls.info.id == id){
			return true;
		}
	}
	return false;
}

FFBoardMain* SelectMain(uint16_t id){
	FFBoardMain* cls = nullptr;
	for(class_entry e : class_registry){
		if(e.info.id == id){
			cls = e.create();
		}
	}
	// Return base class if not found
	if(cls == nullptr){
		cls = new FFBoardMain();
	}
	return cls;

}

