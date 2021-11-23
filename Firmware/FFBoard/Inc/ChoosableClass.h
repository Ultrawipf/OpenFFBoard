/*
 * ChoosableClass.h
 *
 *  Created on: 18.02.2020
 *      Author: Yannick
 */

#ifndef CHOOSABLECLASS_H_
#define CHOOSABLECLASS_H_
#include "ClassIDs.h"

struct ClassIdentifier {
	const char* name;	// Display name of this class
	const char* clsname; // Classname for addressing with string based commands (main or pwmdriver...)
	uint16_t id;	// TODO replace with classchooser id. Unique id for this class. If multiple instances set unique char at runtime
	char unique;
	bool hidden = false;	// Hide from classchooser listings?
};

class ChoosableClass {
public:

	static ClassIdentifier info;

	/**
	 * Returns true if a new instance can be created.
	 * Use this to do prechecks if ressources are available
	 * If it returns false this can signal the classchooser that at this time a new instance can not be created.
	 */
	virtual static bool isCreatable() {return true;};
	virtual const ClassIdentifier getInfo() = 0;

	/**
	 * Type of this class. Mainclass, motordriver...
	 * Should be implemented by the parent class so it is not in the info struct
	 */
	static ClassType getClassType() = 0;

};

#endif /* CHOOSABLECLASS_H_ */
