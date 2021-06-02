/*
 * ChoosableClass.h
 *
 *  Created on: 18.02.2020
 *      Author: Yannick
 */

#ifndef CHOOSABLECLASS_H_
#define CHOOSABLECLASS_H_

struct ClassIdentifier {
	const char* name;
	uint16_t id;
	char unique;
	bool hidden = false;
};

class ChoosableClass {
public:

	static ClassIdentifier info;

	/**
	 * Returns true if a new instance can be created.
	 * Use this to do prechecks if ressources are available
	 * If it returns false this can signal the classchooser that at this time a new instance can not be created.
	 */
	static bool isCreatable() {return true;};
	virtual const ClassIdentifier getInfo() = 0;

};

#endif /* CHOOSABLECLASS_H_ */
