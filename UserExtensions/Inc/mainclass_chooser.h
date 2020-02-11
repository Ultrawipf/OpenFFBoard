/*
 * mainclass_chooser.h
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */

#ifndef MAINCLASS_CHOOSER_H_
#define MAINCLASS_CHOOSER_H_
#include "FFBoardMain.h"
#include "vector"

FFBoardMain* SelectMain(uint16_t id);

struct class_entry
{
	FFBoardMainIdentifier info;
    FFBoardMain *(*create)();
};

template<class T>
class_entry add_class()
{
  return { T::info, []() -> FFBoardMain * { return new T; } };
}

std::string printAvailableClasses();
bool isValidClassId(uint16_t id);

extern std::vector<class_entry> class_registry;

#endif /* MAINCLASS_CHOOSER_H_ */
