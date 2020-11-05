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
	bool hidden = false;
};

class ChoosableClass {
public:

	static ClassIdentifier info;
	virtual const ClassIdentifier getInfo() = 0;
};

#endif /* CHOOSABLECLASS_H_ */
